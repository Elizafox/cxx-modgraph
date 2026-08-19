// SPDX-License-Identifier: 0BSD

#include "cxx_modgraph/facts.hpp"
#include "cxx_modgraph/json.hpp"
#include "cxx_modgraph/make.hpp"
#include "cxx_modgraph/p1689.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string_view>

namespace
{

struct Options
{
    enum class EmitFormat
    {
        json,
        make
    };

    enum class InputFormat
    {
        canonical,
        p1689
    };

    std::filesystem::path input_path;
    std::optional<std::filesystem::path> output_path;
    std::optional<std::filesystem::path> source_root;
    std::optional<std::filesystem::path> compilation_database;
    std::filesystem::path bmi_directory = "build/bmi";
    std::vector<cxx_modgraph::ExternalModule> external_modules;
    bool help = false;
    bool version = false;
    EmitFormat emit_format = EmitFormat::json;
    InputFormat input_format = InputFormat::canonical;
};

void print_usage(std::ostream &output)
{
    output << "Usage: cxx-modgraph --input FILE [OPTIONS]\n"
              "\n"
              "Read, validate, and emit C++ module dependency facts.\n"
              "\n"
              "Options:\n"
              "  -i, --input FILE       Read dependency JSON; use - for stdin\n"
              "      --input-format FMT Read canonical (default) or p1689 JSON\n"
              "      --compdb FILE      Compilation database paired with P1689 input\n"
              "      --bmi-dir DIR      BMI output directory for P1689 providers\n"
              "      --external-module NAME=PATH\n"
              "                           Register a prebuilt/external module\n"
              "  -o, --output FILE      Write output to FILE instead of stdout\n"
              "      --source-root DIR  Override the input's explicit source root\n"
              "      --emit FORMAT      Emit json (default) or make\n"
              "  -h, --help             Show this help\n"
              "      --version          Show the program version\n";
}

std::optional<Options> parse_options(int argc, char **argv)
{
    Options options;
    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument(argv[index]);
        if (argument == "--help" || argument == "-h")
        {
            options.help = true;
        }
        else if (argument == "--version")
        {
            options.version = true;
        }
        else if (argument == "--input" || argument == "-i")
        {
            if (++index == argc)
            {
                std::cerr << "error: " << argument << " requires a value\n";
                return std::nullopt;
            }
            options.input_path = argv[index];
        }
        else if (argument == "--output" || argument == "-o")
        {
            if (++index == argc)
            {
                std::cerr << "error: " << argument << " requires a value\n";
                return std::nullopt;
            }
            options.output_path = argv[index];
        }
        else if (argument == "--input-format")
        {
            if (++index == argc)
            {
                std::cerr << "error: --input-format requires a value\n";
                return std::nullopt;
            }
            const std::string_view format(argv[index]);
            if (format == "canonical")
            {
                options.input_format = Options::InputFormat::canonical;
            }
            else if (format == "p1689")
            {
                options.input_format = Options::InputFormat::p1689;
            }
            else
            {
                std::cerr << "error: unknown input format '" << format << "'\n";
                return std::nullopt;
            }
        }
        else if (argument == "--compdb")
        {
            if (++index == argc)
            {
                std::cerr << "error: --compdb requires a value\n";
                return std::nullopt;
            }
            options.compilation_database = argv[index];
        }
        else if (argument == "--bmi-dir")
        {
            if (++index == argc)
            {
                std::cerr << "error: --bmi-dir requires a value\n";
                return std::nullopt;
            }
            options.bmi_directory = argv[index];
        }
        else if (argument == "--external-module")
        {
            if (++index == argc)
            {
                std::cerr << "error: --external-module requires NAME=PATH\n";
                return std::nullopt;
            }
            const std::string_view mapping(argv[index]);
            const std::size_t separator = mapping.find('=');
            if (separator == 0 || separator == std::string_view::npos ||
                separator + 1 == mapping.size())
            {
                std::cerr << "error: --external-module requires NAME=PATH\n";
                return std::nullopt;
            }
            options.external_modules.push_back({std::string(mapping.substr(0, separator)),
                                                std::string(mapping.substr(separator + 1))});
        }
        else if (argument == "--source-root")
        {
            if (++index == argc)
            {
                std::cerr << "error: --source-root requires a value\n";
                return std::nullopt;
            }
            options.source_root = argv[index];
        }
        else if (argument == "--emit")
        {
            if (++index == argc)
            {
                std::cerr << "error: --emit requires a value\n";
                return std::nullopt;
            }
            const std::string_view format(argv[index]);
            if (format == "json")
            {
                options.emit_format = Options::EmitFormat::json;
            }
            else if (format == "make")
            {
                options.emit_format = Options::EmitFormat::make;
            }
            else
            {
                std::cerr << "error: unknown output format '" << format << "'\n";
                return std::nullopt;
            }
        }
        else
        {
            std::cerr << "error: unknown option '" << argument << "'\n";
            return std::nullopt;
        }
    }
    return options;
}

int run(const Options &options)
{
    std::ifstream input_file;
    std::istream *input = &std::cin;
    if (options.input_path != "-")
    {
        input_file.open(options.input_path);
        if (!input_file)
        {
            std::cerr << "error: cannot open input '" << options.input_path.string() << "'\n";
            return 1;
        }
        input = &input_file;
    }

    std::ostringstream input_contents;
    input_contents << input->rdbuf();

    cxx_modgraph::DependencyFacts facts;
    std::vector<cxx_modgraph::JsonError> parse_errors;
    if (options.input_format == Options::InputFormat::canonical)
    {
        cxx_modgraph::JsonParseResult parsed = cxx_modgraph::parse_json(input_contents.str());
        if (parsed.ok())
        {
            facts = std::move(*parsed.facts);
            facts.external_modules.insert(facts.external_modules.end(),
                                          options.external_modules.begin(),
                                          options.external_modules.end());
        }
        else
        {
            parse_errors = std::move(parsed.errors);
        }
    }
    else if (!options.compilation_database)
    {
        std::cerr << "error: --compdb is required for P1689 input\n";
        return 2;
    }
    else
    {
        std::ifstream commands(*options.compilation_database);
        if (!commands)
        {
            std::cerr << "error: cannot open compilation database '"
                      << options.compilation_database->string() << "'\n";
            return 1;
        }
        std::ostringstream command_contents;
        command_contents << commands.rdbuf();
        cxx_modgraph::P1689ImportOptions import_options;
        import_options.source_root = options.source_root.value_or(".");
        import_options.bmi_directory = options.bmi_directory;
        import_options.external_modules = options.external_modules;
        cxx_modgraph::P1689ImportResult imported = cxx_modgraph::import_p1689(
            input_contents.str(), command_contents.str(), import_options);
        if (imported.ok())
        {
            facts = std::move(*imported.facts);
        }
        else
        {
            parse_errors = std::move(imported.errors);
        }
    }
    if (!parse_errors.empty())
    {
        for (const cxx_modgraph::JsonError &error : parse_errors)
        {
            std::cerr << "error:" << error.line << ':' << error.column << ": " << error.message
                      << '\n';
        }
        return 1;
    }

    if (options.source_root && options.input_format == Options::InputFormat::canonical)
    {
        facts.source_root = *options.source_root;
    }
    const cxx_modgraph::AnalysisResult analysis = cxx_modgraph::analyze(facts);
    if (!analysis.ok())
    {
        for (const cxx_modgraph::Diagnostic &diagnostic : analysis.diagnostics)
        {
            std::cerr << "error: " << diagnostic.message << '\n';
        }
        return 1;
    }

    std::ofstream output_file;
    std::ostream *output = &std::cout;
    if (options.output_path)
    {
        output_file.open(*options.output_path);
        if (!output_file)
        {
            std::cerr << "error: cannot open output '" << options.output_path->string() << "'\n";
            return 1;
        }
        output = &output_file;
    }
    if (options.emit_format == Options::EmitFormat::json)
    {
        cxx_modgraph::write_json(*output, facts);
    }
    else
    {
        cxx_modgraph::write_make(*output, facts);
    }
    return *output ? 0 : 1;
}

} // namespace

int main(int argc, char **argv)
{
    const std::optional<Options> options = parse_options(argc, argv);
    if (!options)
    {
        return 2;
    }
    if (options->help)
    {
        print_usage(std::cout);
        return 0;
    }
    if (options->version)
    {
        std::cout << "cxx-modgraph 0.1.0\n";
        return 0;
    }
    if (options->input_path.empty())
    {
        std::cerr << "error: --input is required\n";
        print_usage(std::cerr);
        return 2;
    }
    return run(*options);
}
