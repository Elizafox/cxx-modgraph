// SPDX-License-Identifier: 0BSD

#include "cxx_modgraph/facts.hpp"
#include "cxx_modgraph/json.hpp"
#include "cxx_modgraph/make.hpp"
#include "cxx_modgraph/ninja.hpp"
#include "cxx_modgraph/p1689.hpp"

#include <CLI/CLI.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{

struct Options
{
    enum class EmitFormat
    {
        json,
        make,
        ninja
    };

    enum class InputFormat
    {
        canonical,
        p1689
    };

    std::vector<std::filesystem::path> input_paths;
    std::optional<std::filesystem::path> output_path;
    std::optional<std::filesystem::path> source_root;
    std::optional<std::filesystem::path> compilation_database;
    std::filesystem::path bmi_directory = "build/bmi";
    std::string bmi_extension = ".pcm";
    std::vector<cxx_modgraph::ExternalModule> external_modules;
    EmitFormat emit_format = EmitFormat::json;
    InputFormat input_format = InputFormat::canonical;
};

std::optional<int> parse_options(int argc, char **argv, Options &options)
{
    CLI::App app{"Read, validate, and emit C++ module dependency facts.", "cxx-modgraph"};
    std::string input_format = "canonical";
    std::string emit_format = "json";

    app.add_option("-i,--input", options.input_paths,
                   "Read dependency JSON; repeat for P1689 inputs")
        ->type_name("FILE")
        ->required();
    app.add_option("--input-format", input_format, "Input JSON format")
        ->type_name("FMT")
        ->check(CLI::IsMember({"canonical", "p1689"}))
        ->default_str("canonical");
    app.add_option("--compdb", options.compilation_database,
                   "Compilation database paired with P1689 input")
        ->type_name("FILE");
    app.add_option("--bmi-dir", options.bmi_directory, "BMI output directory for P1689 providers")
        ->type_name("DIR")
        ->default_str("build/bmi");
    app.add_option("--bmi-extension", options.bmi_extension, "BMI filename suffix")
        ->type_name("X")
        ->default_str(".pcm");
    app.add_option_function<std::pair<std::string, std::filesystem::path>>(
           "--external-module", [&options](const auto &mapping)
           { options.external_modules.push_back({mapping.first, mapping.second}); },
           "Register a prebuilt/external module")
        ->type_name("NAME=PATH")
        ->delimiter('=')
        ->trigger_on_parse();
    app.add_option("-o,--output", options.output_path, "Write output instead of stdout")
        ->type_name("FILE");
    app.add_option("--source-root", options.source_root,
                   "Override the input's explicit source root")
        ->type_name("DIR");
    app.add_option("--emit", emit_format, "Output format")
        ->type_name("FORMAT")
        ->check(CLI::IsMember({"json", "make", "ninja"}))
        ->default_str("json");
    app.set_version_flag("--version", "cxx-modgraph 0.1.0");

    try
    {
        app.parse(argc, argv);
    }
    catch (const CLI::ParseError &error)
    {
        return app.exit(error);
    }

    options.input_format =
        input_format == "p1689" ? Options::InputFormat::p1689 : Options::InputFormat::canonical;
    if (emit_format == "make")
    {
        options.emit_format = Options::EmitFormat::make;
    }
    else if (emit_format == "ninja")
    {
        options.emit_format = Options::EmitFormat::ninja;
    }

    return std::nullopt;
}

int run(const Options &options)
{
    if (options.input_format == Options::InputFormat::canonical && options.input_paths.size() != 1)
    {
        std::cerr << "error: repeated --input is only supported for P1689 input\n";
        return 2;
    }

    cxx_modgraph::DependencyFacts facts;
    std::vector<cxx_modgraph::JsonError> parse_errors;
    if (options.input_format == Options::InputFormat::canonical)
    {
        std::ifstream input_file;
        std::istream *input = &std::cin;
        if (options.input_paths.front() != "-")
        {
            input_file.open(options.input_paths.front());
            if (!input_file)
            {
                std::cerr << "error: cannot open input '" << options.input_paths.front().string()
                          << "'\n";
                return 1;
            }

            input = &input_file;
        }

        std::ostringstream input_contents;
        input_contents << input->rdbuf();
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
        import_options.bmi_extension = options.bmi_extension;
        facts.source_root = import_options.source_root;
        facts.external_modules = options.external_modules;
        for (const std::filesystem::path &input_path : options.input_paths)
        {
            if (input_path == "-" && options.input_paths.size() != 1)
            {
                std::cerr << "error: stdin cannot be combined with multiple P1689 inputs\n";
                return 2;
            }

            std::ifstream input_file;
            std::istream *input = &std::cin;
            if (input_path != "-")
            {
                input_file.open(input_path);
                if (!input_file)
                {
                    std::cerr << "error: cannot open input '" << input_path.string() << "'\n";
                    return 1;
                }

                input = &input_file;
            }

            std::ostringstream input_contents;
            input_contents << input->rdbuf();
            cxx_modgraph::P1689ImportResult imported = cxx_modgraph::import_p1689(
                input_contents.str(), command_contents.str(), import_options);
            if (imported.ok())
            {
                auto &units = imported.facts->translation_units;
                facts.translation_units.insert(facts.translation_units.end(),
                                               std::make_move_iterator(units.begin()),
                                               std::make_move_iterator(units.end()));
            }
            else
            {
                parse_errors.insert(parse_errors.end(),
                                    std::make_move_iterator(imported.errors.begin()),
                                    std::make_move_iterator(imported.errors.end()));
            }
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
    else if (options.emit_format == Options::EmitFormat::make)
    {
        cxx_modgraph::write_make(*output, facts);
    }
    else
    {
        cxx_modgraph::write_ninja(*output, facts);
    }

    return *output ? 0 : 1;
}

} // namespace

int main(int argc, char **argv)
{
    Options options;
    const std::optional<int> parse_result = parse_options(argc, argv, options);
    if (parse_result)
    {
        return *parse_result;
    }

    return run(options);
}
