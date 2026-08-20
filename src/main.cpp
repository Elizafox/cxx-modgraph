// SPDX-License-Identifier: 0BSD

#include "cxx_modgraph/facts.hpp"
#include "cxx_modgraph/incremental.hpp"
#include "cxx_modgraph/json.hpp"
#include "cxx_modgraph/make.hpp"
#include "cxx_modgraph/ninja.hpp"
#include "cxx_modgraph/p1689.hpp"
#include "cxx_modgraph/p2977.hpp"
#include "cxx_modgraph/package_metadata.hpp"

#include <CLI/CLI.hpp>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{

struct Options
{
    enum class Query
    {
        none,
        explain_module,
        why,
        cycle,
        providers,
        critical_path
    };
    enum class EmitFormat
    {
        json,
        make,
        ninja,
        ninja_dyndep,
        p2977
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
    std::vector<std::filesystem::path> package_metadata;
    EmitFormat emit_format = EmitFormat::json;
    InputFormat input_format = InputFormat::canonical;
    Query query = Query::none;
    std::string query_module;
    std::filesystem::path query_source;
    bool check_fresh = false;
    bool daemon = false;
    std::string scanner;
    std::string scanner_version;
    cxx_modgraph::BmiCompatibility compatibility;
};

std::string digest(std::string_view value)
{
    std::uint64_t hash = 14695981039346656037ull;
    for (unsigned char c : value)
    {
        hash ^= c;
        hash *= 1099511628211ull;
    }
    std::ostringstream out;
    out << "fnv1a64:" << std::hex << std::setfill('0') << std::setw(16) << hash;
    return out.str();
}

std::string unit_digest(const cxx_modgraph::TranslationUnit &unit)
{
    std::ostringstream out;
    const auto add = [&](std::string_view value) { out << value.size() << ':' << value; };
    add(unit.source_path.lexically_normal().generic_string());
    add(unit.object_path.lexically_normal().generic_string());
    add(unit.module_set);
    add(unit.work_directory.lexically_normal().generic_string());
    for (const auto &module : unit.provides)
    {
        add(module.name);
        add(module.bmi_path.generic_string());
    }
    for (const auto &module : unit.required_modules)
        add(module);
    for (const auto &argument : unit.arguments)
        add(argument);
    for (const auto &argument : unit.local_arguments)
        add(argument);
    add(unit.bmi_compatibility.compiler_executable);
    add(unit.bmi_compatibility.compiler_version);
    add(unit.bmi_compatibility.target_triple);
    add(unit.bmi_compatibility.sysroot);
    add(unit.bmi_compatibility.configuration);
    out << unit.is_private;
    return digest(out.str());
}

bool has_explicit_compatibility(const cxx_modgraph::BmiCompatibility &c)
{
    return c.configuration != "default" || !c.compiler_executable.empty() ||
           !c.compiler_version.empty() || !c.target_triple.empty() || !c.sysroot.empty() ||
           !c.language_standard.empty() || !c.standard_library.empty() || !c.user_key.empty() ||
           !c.adapter_keys.empty();
}

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
    app.add_option("--scanner", options.scanner, "Dependency scanner identity")->type_name("NAME");
    app.add_option("--scanner-version", options.scanner_version, "Dependency scanner version")
        ->type_name("VERSION");
    app.add_option("--configuration", options.compatibility.configuration,
                   "Build configuration identifier (for example debug or asan)")
        ->type_name("NAME")
        ->default_str("default");
    app.add_option("--bmi-compatibility-key", options.compatibility.user_key,
                   "Build-system supplied BMI compatibility policy key")
        ->type_name("VALUE");
    app.add_option("--compiler-executable", options.compatibility.compiler_executable,
                   "Compiler executable identity for BMI compatibility")
        ->type_name("ID");
    app.add_option("--compiler-version", options.compatibility.compiler_version,
                   "Compiler version for BMI compatibility")
        ->type_name("VERSION");
    app.add_option("--target-triple", options.compatibility.target_triple,
                   "Target triple for BMI compatibility")
        ->type_name("TRIPLE");
    app.add_option("--sysroot-identifier", options.compatibility.sysroot,
                   "Sysroot identity for BMI compatibility")
        ->type_name("ID");
    app.add_option("--language-standard", options.compatibility.language_standard,
                   "Language standard for BMI compatibility")
        ->type_name("STANDARD");
    app.add_option("--standard-library", options.compatibility.standard_library,
                   "Standard library for BMI compatibility")
        ->type_name("LIBRARY");
    app.add_option("--adapter-compatibility-key", options.compatibility.adapter_keys,
                   "Compiler-adapter-specific BMI compatibility material; repeat as needed")
        ->type_name("VALUE");
    app.add_flag("--check-fresh", options.check_fresh, "Fail if any recorded graph input changed");
    app.add_flag("--daemon", options.daemon,
                 "Keep running and reload canonical fact files named on stdin");
    app.add_option_function<std::pair<std::string, std::filesystem::path>>(
           "--external-module", [&options](const auto &mapping)
           { options.external_modules.push_back({mapping.first, mapping.second}); },
           "Register a prebuilt/external module")
        ->type_name("NAME=PATH")
        ->delimiter('=')
        ->trigger_on_parse();
    app.add_option("--package-metadata", options.package_metadata,
                   "Merge installed module-source metadata; repeat for dependencies")
        ->type_name("FILE");
    app.add_option("-o,--output", options.output_path, "Write output instead of stdout")
        ->type_name("FILE");
    app.add_option("--source-root", options.source_root,
                   "Override the input's explicit source root")
        ->type_name("DIR");
    app.add_option("--emit", emit_format, "Output format")
        ->type_name("FORMAT")
        ->check(CLI::IsMember({"json", "make", "ninja", "ninja-dyndep", "p2977"}))
        ->default_str("json");
    app.set_version_flag("--version", "cxx-modgraph 0.1.0");

    auto *explain = app.add_subcommand("explain", "Explain graph facts");
    auto *explain_module = explain->add_subcommand("module", "Explain a module");
    explain_module->add_option("name", options.query_module)->required();
    explain_module->callback([&options] { options.query = Options::Query::explain_module; });
    auto *why = app.add_subcommand("why", "Explain a direct module requirement");
    why->add_option("source", options.query_source)->required();
    why->add_option("module", options.query_module)->required();
    why->callback([&options] { options.query = Options::Query::why; });
    auto *cycle = app.add_subcommand("cycle", "Print a concrete cycle witness");
    cycle->callback([&options] { options.query = Options::Query::cycle; });
    auto *providers = app.add_subcommand("providers", "List every provider for a module");
    providers->add_option("module", options.query_module)->required();
    providers->callback([&options] { options.query = Options::Query::providers; });
    auto *critical = app.add_subcommand("critical-path", "Print the longest dependency chain");
    critical->callback([&options] { options.query = Options::Query::critical_path; });

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
    else if (emit_format == "ninja-dyndep")
    {
        options.emit_format = Options::EmitFormat::ninja_dyndep;
    }
    else if (emit_format == "p2977")
    {
        options.emit_format = Options::EmitFormat::p2977;
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
            if (has_explicit_compatibility(options.compatibility))
            {
                for (auto &unit : facts.translation_units)
                    unit.bmi_compatibility = options.compatibility;
                for (auto &module : facts.external_modules)
                    module.bmi_compatibility = options.compatibility;
            }
            if (options.check_fresh)
            {
                if (facts.inputs.empty())
                {
                    std::cerr << "error: graph has no recorded input digests\n";
                    return 1;
                }
                for (const auto &artifact : facts.inputs)
                {
                    std::ifstream f(artifact.path, std::ios::binary);
                    std::ostringstream s;
                    if (!f)
                    {
                        std::cerr << "error: graph input '" << artifact.path.string()
                                  << "' is missing\n";
                        return 1;
                    }
                    s << f.rdbuf();
                    if (digest(s.str()) != artifact.digest)
                    {
                        std::cerr << "error: graph is stale: '" << artifact.path.string()
                                  << "' changed\n";
                        return 1;
                    }
                }
            }
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
        import_options.scanner = options.scanner;
        import_options.scanner_version = options.scanner_version;
        import_options.bmi_compatibility = options.compatibility;
        facts.source_root = import_options.source_root;
        facts.external_modules = options.external_modules;
        facts.inputs.push_back({*options.compilation_database, digest(command_contents.str())});
        for (const std::filesystem::path &input_path : options.input_paths)
        {
            import_options.rule_source = input_path;
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
                facts.version = cxx_modgraph::current_facts_version;
                facts.inputs.push_back({input_path, digest(input_contents.str())});
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

    for (const auto &metadata_path : options.package_metadata)
    {
        std::ifstream metadata(metadata_path);
        if (!metadata)
        {
            std::cerr << "error: cannot open package metadata '" << metadata_path.string() << "'\n";
            return 1;
        }
        std::ostringstream contents;
        contents << metadata.rdbuf();
        auto imported = cxx_modgraph::import_package_metadata(contents.str(), metadata_path);
        if (!imported.ok())
        {
            for (const auto &error : imported.errors)
                std::cerr << "error:" << error.line << ':' << error.column << ": " << error.message
                          << '\n';
            return 1;
        }
        auto &package = *imported.facts;
        facts.translation_units.insert(facts.translation_units.end(),
                                       std::make_move_iterator(package.translation_units.begin()),
                                       std::make_move_iterator(package.translation_units.end()));
        facts.module_sets.insert(facts.module_sets.end(),
                                 std::make_move_iterator(package.module_sets.begin()),
                                 std::make_move_iterator(package.module_sets.end()));
        facts.inputs.push_back({metadata_path, digest(contents.str())});
    }

    if (options.source_root && options.input_format == Options::InputFormat::canonical)
    {
        facts.source_root = *options.source_root;
    }

    const cxx_modgraph::AnalysisResult analysis = cxx_modgraph::analyze(facts);
    if (options.daemon)
    {
        if (options.input_format != Options::InputFormat::canonical ||
            options.input_paths.front() == "-")
        {
            std::cerr << "error: --daemon requires an initial canonical file input (not stdin)\n";
            return 2;
        }
        if (!analysis.ok())
        {
            for (const auto &diagnostic : analysis.diagnostics)
                std::cerr << "error: " << diagnostic.message << '\n';
            return 1;
        }
        cxx_modgraph::IncrementalState state(std::move(facts));
        std::string command;
        while (std::getline(std::cin, command))
        {
            if (command == "quit")
                break;
            std::ifstream snapshot(command);
            auto parsed =
                snapshot ? cxx_modgraph::read_json(snapshot) : cxx_modgraph::JsonParseResult{};
            if (!snapshot || !parsed.ok())
            {
                std::cout << "{\"ok\":false,\"error\":\"cannot load canonical snapshot\"}\n"
                          << std::flush;
                continue;
            }
            std::set<std::string> next_sources;
            bool topology_changed = false;
            std::set<std::string> affected;
            for (auto &unit : parsed.facts->translation_units)
            {
                next_sources.insert(unit.source_path.lexically_normal().generic_string());
                const auto current =
                    std::ranges::find_if(state.facts().translation_units,
                                         [&](const auto &old)
                                         {
                                             return old.source_path.lexically_normal() ==
                                                    unit.source_path.lexically_normal();
                                         });
                if (current != state.facts().translation_units.end() &&
                    unit_digest(*current) == unit_digest(unit))
                    continue;
                auto report = state.update(std::move(unit));
                topology_changed = topology_changed || report.topology_changed;
                affected.insert(report.affected_translation_units.begin(),
                                report.affected_translation_units.end());
            }
            std::vector<std::filesystem::path> removed;
            for (const auto &unit : state.facts().translation_units)
                if (!next_sources.contains(unit.source_path.lexically_normal().generic_string()))
                    removed.push_back(unit.source_path);
            for (const auto &source : removed)
            {
                auto report = state.erase(source);
                topology_changed = topology_changed || report.topology_changed;
                affected.insert(report.affected_translation_units.begin(),
                                report.affected_translation_units.end());
            }
            std::cout << "{\"ok\":" << (state.analysis().ok() ? "true" : "false")
                      << ",\"topology-changed\":" << (topology_changed ? "true" : "false")
                      << ",\"affected\":[";
            std::size_t index = 0;
            for (const auto &source : affected)
                std::cout << (index++ ? "," : "") << std::quoted(source);
            std::cout << "]}\n" << std::flush;
        }
        return 0;
    }
    if (options.query != Options::Query::none)
    {
        auto print_path = [](const std::vector<cxx_modgraph::NodeId> &path)
        {
            for (std::size_t i = 0; i < path.size(); ++i)
                std::cout << (i ? " -> " : "") << path[i];
            std::cout << '\n';
        };
        if (options.query == Options::Query::cycle)
        {
            auto path = analysis.graph.cycle_witness();
            if (path.empty())
            {
                std::cout << "no cycle\n";
                return 0;
            }
            print_path(path);
            return 1;
        }
        if (options.query == Options::Query::critical_path)
        {
            auto path = analysis.graph.critical_path();
            if (path.empty() && analysis.graph.topological_sort().has_cycle())
            {
                std::cerr << "error: critical path is undefined for a cyclic graph\n";
                return 1;
            }
            print_path(path);
            return 0;
        }
        bool found = false;
        for (const auto &u : facts.translation_units)
        {
            bool provides = false;
            for (const auto &p : u.provides)
                if (p.name == options.query_module)
                    provides = true;
            if (options.query == Options::Query::providers && provides)
            {
                std::cout << options.query_module << " provided by " << u.source_path.string();
                if (u.provenance)
                    std::cout << " (output " << u.provenance->original_output.string()
                              << ", scanner " << u.provenance->scanner << ' '
                              << u.provenance->scanner_version << ')';
                std::cout << '\n';
                found = true;
            }
            if (options.query == Options::Query::explain_module && provides)
            {
                std::cout << "module " << options.query_module
                          << "\nprovider: " << u.source_path.string() << "\nbmi: ";
                for (const auto &p : u.provides)
                    if (p.name == options.query_module)
                        std::cout << p.bmi_path.string();
                std::cout << '\n';
                found = true;
            }
            if (options.query == Options::Query::why &&
                u.source_path.lexically_normal() == options.query_source.lexically_normal())
            {
                for (const auto &r : u.dependency_reasons)
                    if (r.module == options.query_module)
                    {
                        std::cout << u.source_path.string() << " requires " << r.module << ": "
                                  << r.reason << " (lookup: " << r.lookup_method << ")\n";
                        found = true;
                    }
                if (!found && std::ranges::find(u.required_modules, options.query_module) !=
                                  u.required_modules.end())
                {
                    std::cout << u.source_path.string() << " directly requires "
                              << options.query_module << " (canonical facts)\n";
                    found = true;
                }
            }
        }
        for (const auto &e : facts.external_modules)
            if (options.query == Options::Query::providers && e.name == options.query_module)
            {
                std::cout << e.name << " provided externally by " << e.bmi_path.string() << '\n';
                found = true;
            }
        if (!found)
        {
            std::cerr << "error: no matching graph fact\n";
            return 1;
        }
        return 0;
    }
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
    else if (options.emit_format == Options::EmitFormat::ninja)
    {
        cxx_modgraph::write_ninja(*output, facts);
    }
    else if (options.emit_format == Options::EmitFormat::ninja_dyndep)
    {
        cxx_modgraph::write_ninja_dyndep(*output, facts);
    }
    else
    {
        cxx_modgraph::write_p2977(*output, facts);
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
