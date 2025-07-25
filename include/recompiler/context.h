#ifndef __RECOMP_PORT__
#define __RECOMP_PORT__

#include <span>
#include <string_view>
#include <cstdint>
#include <utility>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#include <optional>

#ifdef _MSC_VER
inline uint32_t byteswap(uint32_t val) {
    return _byteswap_ulong(val);
}
#else
constexpr uint32_t byteswap(uint32_t val) {
    return __builtin_bswap32(val);
}
#endif

namespace N64Recomp {
    struct Function {
        uint32_t vram;
        uint32_t rom;
        std::vector<uint32_t> words;
        std::string name;
        uint16_t section_index;
        bool ignored;
        bool reimplemented;
        bool stubbed;
        std::unordered_map<int32_t, std::string> function_hooks;

        Function(uint32_t vram, uint32_t rom, std::vector<uint32_t> words, std::string name, uint16_t section_index, bool ignored = false, bool reimplemented = false, bool stubbed = false)
                : vram(vram), rom(rom), words(std::move(words)), name(std::move(name)), section_index(section_index), ignored(ignored), reimplemented(reimplemented), stubbed(stubbed) {}
        Function() = default;
    };
    
    struct JumpTable {
        uint32_t vram;
        uint32_t addend_reg;
        uint32_t rom;
        uint32_t lw_vram;
        uint32_t addu_vram;
        uint32_t jr_vram;
        uint16_t section_index;
        std::optional<uint32_t> got_offset;
        std::vector<uint32_t> entries;

        JumpTable(uint32_t vram, uint32_t addend_reg, uint32_t rom, uint32_t lw_vram, uint32_t addu_vram, uint32_t jr_vram, uint16_t section_index, std::optional<uint32_t> got_offset, std::vector<uint32_t>&& entries)
                : vram(vram), addend_reg(addend_reg), rom(rom), lw_vram(lw_vram), addu_vram(addu_vram), jr_vram(jr_vram), section_index(section_index), got_offset(got_offset), entries(std::move(entries)) {}
    };

    enum class RelocType : uint8_t {
        R_MIPS_NONE = 0,
        R_MIPS_16,
        R_MIPS_32,
        R_MIPS_REL32,
        R_MIPS_26,
        R_MIPS_HI16,
        R_MIPS_LO16,
        R_MIPS_GPREL16,
    };

    struct Reloc {
        uint32_t address;
        uint32_t target_section_offset;
        uint32_t symbol_index; // Only used for reference symbols and special section symbols
        uint16_t target_section;
        RelocType type;
        bool reference_symbol;
    };

    // Special section indices.
    constexpr uint16_t SectionAbsolute = (uint16_t)-2;
    constexpr uint16_t SectionImport = (uint16_t)-3; // Imported symbols for mods
    constexpr uint16_t SectionEvent = (uint16_t)-4;

    // Special section names.
    constexpr std::string_view PatchSectionName = ".recomp_patch";
    constexpr std::string_view ForcedPatchSectionName = ".recomp_force_patch";
    constexpr std::string_view ExportSectionName = ".recomp_export";
    constexpr std::string_view EventSectionName = ".recomp_event";
    constexpr std::string_view ImportSectionPrefix = ".recomp_import.";
    constexpr std::string_view CallbackSectionPrefix = ".recomp_callback.";
    constexpr std::string_view HookSectionPrefix = ".recomp_hook.";
    constexpr std::string_view HookReturnSectionPrefix = ".recomp_hook_return.";

    // Special dependency names.
    constexpr std::string_view DependencySelf = ".";
    constexpr std::string_view DependencyBaseRecomp = "*";

    struct Section {
        uint32_t rom_addr = 0;
        uint32_t ram_addr = 0;
        uint32_t size = 0;
        uint32_t bss_size = 0; // not populated when using a symbol toml
        std::vector<uint32_t> function_addrs; // only used by the CLI (to find the size of static functions)
        std::vector<Reloc> relocs;
        std::string name;
        uint16_t bss_section_index = (uint16_t)-1;
        bool executable = false;
        bool relocatable = false; // TODO is this needed? relocs being non-empty should be an equivalent check.
        bool has_mips32_relocs = false;
        bool fixed_address = false; // Only used in mods, indicates that the section shouldn't be relocated or placed into mod memory.
        bool globally_loaded = false; // Only used in mods, indicates that the section's functions should be globally loaded. Does not actually load the section's contents into ram.
        std::optional<uint32_t> got_ram_addr = std::nullopt;
    };

    struct ReferenceSection {
        uint32_t rom_addr;
        uint32_t ram_addr;
        uint32_t size;
        bool relocatable;
    };

    struct ReferenceSymbol {
        std::string name;
        uint16_t section_index;
        uint32_t section_offset;
        bool is_function;
    };

    struct ElfParsingConfig {
        std::string bss_section_suffix;
        // Functions with manual size overrides
        std::unordered_map<std::string, size_t> manually_sized_funcs;
        // The section names that were specified as relocatable
        std::unordered_set<std::string> relocatable_sections;
        bool has_entrypoint;
        int32_t entrypoint_address;
        bool use_absolute_symbols;
        bool unpaired_lo16_warnings;
        bool all_sections_relocatable;
    };
    
    struct DataSymbol {
        uint32_t vram;
        std::string name;

        DataSymbol(uint32_t vram, std::string&& name) : vram(vram), name(std::move(name)) {}
    };

    using DataSymbolMap = std::unordered_map<uint16_t, std::vector<DataSymbol>>;

    extern const std::unordered_set<std::string> reimplemented_funcs;
    extern const std::unordered_set<std::string> ignored_funcs;
    extern const std::unordered_set<std::string> renamed_funcs;

    struct ImportSymbol {
        ReferenceSymbol base;
        size_t dependency_index;
    };

    struct DependencyEvent {
        size_t dependency_index;
        std::string event_name;
    };

    struct EventSymbol {
        ReferenceSymbol base;
    };

    struct Callback {
        size_t function_index;
        size_t dependency_event_index;
    };

    struct SymbolReference {
        // Reference symbol section index, or one of the special section indices such as SectionImport.
        uint16_t section_index;
        size_t symbol_index;
    };

    enum class ReplacementFlags : uint32_t {
        Force = 1 << 0,
    };
    inline ReplacementFlags operator&(ReplacementFlags lhs, ReplacementFlags rhs) { return ReplacementFlags(uint32_t(lhs) & uint32_t(rhs)); }
    inline ReplacementFlags operator|(ReplacementFlags lhs, ReplacementFlags rhs) { return ReplacementFlags(uint32_t(lhs) | uint32_t(rhs)); }

    struct FunctionReplacement {
        uint32_t func_index;
        uint32_t original_section_vrom;
        uint32_t original_vram;
        ReplacementFlags flags;
    };

    enum class HookFlags : uint32_t {
        AtReturn = 1 << 0,
    };
    inline HookFlags operator&(HookFlags lhs, HookFlags rhs) { return HookFlags(uint32_t(lhs) & uint32_t(rhs)); }
    inline HookFlags operator|(HookFlags lhs, HookFlags rhs) { return HookFlags(uint32_t(lhs) | uint32_t(rhs)); }

    struct FunctionHook {
        uint32_t func_index;
        uint32_t original_section_vrom;
        uint32_t original_vram;
        HookFlags flags;
    };

    class Context {
    private:
        //// Reference symbols (used for populating relocations for patches)
        // A list of the sections that contain the reference symbols.
        std::vector<ReferenceSection> reference_sections;
        // A list of the reference symbols.
        std::vector<ReferenceSymbol> reference_symbols;
        // Mapping of symbol name to reference symbol index.
        std::unordered_map<std::string, SymbolReference> reference_symbols_by_name;
        // Whether all reference sections should be treated as relocatable (used in live recompilation).
        bool all_reference_sections_relocatable = false;
    public:
        std::vector<Section> sections;
        std::vector<Function> functions;
        // A list of the list of each function (by index in `functions`) in a given section
        std::vector<std::vector<size_t>> section_functions;
        // A mapping of vram address to every function with that address.
        std::unordered_map<uint32_t, std::vector<size_t>> functions_by_vram;
        // A mapping of bss section index to the corresponding non-bss section index.
        std::unordered_map<uint16_t, uint16_t> bss_section_to_section;
        // The target ROM being recompiled, TODO move this outside of the context to avoid making a copy for mod contexts.
        // Used for reading relocations and for the output binary feature.
        std::vector<uint8_t> rom;
        // Whether reference symbols should be validated when emitting function calls during recompilation.
        bool skip_validating_reference_symbols = true;
        // Whether all function calls (excluding reference symbols) should go through lookup.
        bool use_lookup_for_all_function_calls = false;

        //// Only used by the CLI, TODO move this to a struct in the internal headers.
        // A mapping of function name to index in the functions vector
        std::unordered_map<std::string, size_t> functions_by_name;

        //// Mod dependencies and their symbols
        
        //// Imported values
        // Dependency names.
        std::vector<std::string> dependencies;
        // Mapping of dependency name to dependency index.
        std::unordered_map<std::string, size_t> dependencies_by_name;
        // List of symbols imported from dependencies.
        std::vector<ImportSymbol> import_symbols;
        // List of events imported from dependencies.
        std::vector<DependencyEvent> dependency_events;
        // Mappings of dependency event name to the index in dependency_events, all indexed by dependency.
        std::vector<std::unordered_map<std::string, size_t>> dependency_events_by_name;
        // Mappings of dependency import name to index in import_symbols, all indexed by dependency.
        std::vector<std::unordered_map<std::string, size_t>> dependency_imports_by_name;

        //// Exported values
        // List of function replacements, which contains the original function to replace and the function index to replace it with.
        std::vector<FunctionReplacement> replacements;
        // Indices of every exported function.
        std::vector<size_t> exported_funcs;
        // List of callbacks, which contains the function for the callback and the dependency event it attaches to.
        std::vector<Callback> callbacks;
        // List of symbols from events, which contains the names of events that this context provides.
        std::vector<EventSymbol> event_symbols;
        // List of hooks, which contains the original function to hook and the function index to call at the hook.
        std::vector<FunctionHook> hooks;

        // Causes functions to print their name to the console the first time they're called.
        bool trace_mode;

        // Imports sections and function symbols from a provided context into this context's reference sections and reference functions.
        bool import_reference_context(const Context& reference_context);
        // Reads a data symbol file and adds its contents into this context's reference data symbols.
        bool read_data_reference_syms(const std::filesystem::path& data_syms_file_path);

        static bool from_symbol_file(const std::filesystem::path& symbol_file_path, std::vector<uint8_t>&& rom, Context& out, bool with_relocs);
        static bool from_elf_file(const std::filesystem::path& elf_file_path, Context& out, const ElfParsingConfig& flags, bool for_dumping_context, DataSymbolMap& data_syms_out, bool& found_entrypoint_out);

        Context() = default;

        bool add_dependency(const std::string& id) {
            if (dependencies_by_name.contains(id)) {
                return false;
            }

            size_t dependency_index = dependencies_by_name.size();

            dependencies.emplace_back(id);
            dependencies_by_name.emplace(id, dependency_index);
            dependency_events_by_name.resize(dependencies_by_name.size());
            dependency_imports_by_name.resize(dependencies_by_name.size());

            return true;
        }

        bool add_dependencies(const std::vector<std::string>& new_dependencies) {
            dependencies_by_name.reserve(dependencies_by_name.size() + new_dependencies.size());

            // Check if any of the dependencies already exist and fail if so.
            for (const std::string& dep : new_dependencies) {
                if (dependencies_by_name.contains(dep)) {
                    return false;
                }
            }

            for (const std::string& dep : new_dependencies) {
                size_t dependency_index = dependencies_by_name.size();
                dependencies.emplace_back(dep);
                dependencies_by_name.emplace(dep, dependency_index);
            }

            dependency_events_by_name.resize(dependencies_by_name.size());
            dependency_imports_by_name.resize(dependencies_by_name.size());
            return true;
        }

        bool find_dependency(const std::string& mod_id, size_t& dependency_index) {
            auto find_it = dependencies_by_name.find(mod_id);
            if (find_it != dependencies_by_name.end()) {
                dependency_index = find_it->second;
            }
            else {
                // Handle special dependency names.
                if (mod_id == DependencySelf || mod_id == DependencyBaseRecomp) {
                    add_dependency(mod_id);
                    dependency_index = dependencies_by_name[mod_id];
                }
                else {
                    return false;
                }
            }
            return true;
        }

        size_t find_function_by_vram_section(uint32_t vram, size_t section_index) const {
            auto find_it = functions_by_vram.find(vram);
            if (find_it == functions_by_vram.end()) {
                return (size_t)-1;
            }

            for (size_t function_index : find_it->second) {
                if (functions[function_index].section_index == section_index) {
                    return function_index;
                }
            }

            return (size_t)-1;
        }

        bool has_reference_symbols() const {
            return !reference_symbols.empty() || !import_symbols.empty() || !event_symbols.empty();
        }

        bool is_regular_reference_section(uint16_t section_index) const {
            return section_index != SectionImport && section_index != SectionEvent;
        }

        bool find_reference_symbol(const std::string& symbol_name, SymbolReference& ref_out) const {
            auto find_sym_it = reference_symbols_by_name.find(symbol_name);

            // Check if the symbol was found.
            if (find_sym_it == reference_symbols_by_name.end()) {
                return false;
            }

            ref_out = find_sym_it->second;
            return true;
        }

        bool reference_symbol_exists(const std::string& symbol_name) const {
            SymbolReference dummy_ref;
            return find_reference_symbol(symbol_name, dummy_ref);
        }

        bool find_regular_reference_symbol(const std::string& symbol_name, SymbolReference& ref_out) const {
            SymbolReference ref_found;
            if (!find_reference_symbol(symbol_name, ref_found)) {
                return false;
            }

            // Ignore reference symbols in special sections.
            if (!is_regular_reference_section(ref_found.section_index)) {
                return false;
            }

            ref_out = ref_found;
            return true;
        }

        const ReferenceSymbol& get_reference_symbol(uint16_t section_index, size_t symbol_index) const {
            if (section_index == SectionImport) {
                return import_symbols[symbol_index].base;
            }
            else if (section_index == SectionEvent) {
                return event_symbols[symbol_index].base;
            }
            return reference_symbols[symbol_index];
        }

        size_t num_regular_reference_symbols() {
            return reference_symbols.size();
        }

        const ReferenceSymbol& get_regular_reference_symbol(size_t index) const {
            return reference_symbols[index];
        }

        const ReferenceSymbol& get_reference_symbol(const SymbolReference& ref) const {
            return get_reference_symbol(ref.section_index, ref.symbol_index);
        }

        bool is_reference_section_relocatable(uint16_t section_index) const {
            if (all_reference_sections_relocatable) {
                return true;
            }
            if (section_index == SectionAbsolute) {
                return false;
            }
            else if (section_index == SectionImport || section_index == SectionEvent) {
                return true;
            }
            return reference_sections[section_index].relocatable;
        }

        bool add_reference_symbol(const std::string& symbol_name, uint16_t section_index, uint32_t vram, bool is_function) {
            uint32_t section_vram;

            if (section_index == SectionAbsolute) {
                section_vram = 0;
            }
            else if (section_index < reference_sections.size()) {
                section_vram = reference_sections[section_index].ram_addr;
            }
            // Invalid section index.
            else {
                return false;
            }

            // TODO Check if reference_symbols_by_name already contains the name and show a conflict error if so.
            reference_symbols_by_name.emplace(symbol_name, N64Recomp::SymbolReference{
                .section_index = section_index,
                .symbol_index = reference_symbols.size()
            });

            reference_symbols.emplace_back(N64Recomp::ReferenceSymbol{
                .name = symbol_name,
                .section_index = section_index,
                .section_offset = vram - section_vram,
                .is_function = is_function
            });
            return true;
        }

        void add_import_symbol(const std::string& symbol_name, size_t dependency_index) {
            // TODO Check if dependency_imports_by_name[dependency_index] already contains the name and show a conflict error if so.
            dependency_imports_by_name[dependency_index][symbol_name] = import_symbols.size();
            import_symbols.emplace_back(
                N64Recomp::ImportSymbol {
                    .base = N64Recomp::ReferenceSymbol {
                        .name = symbol_name,
                        .section_index = N64Recomp::SectionImport,
                        .section_offset = 0,
                        .is_function = true
                    },
                    .dependency_index = dependency_index,
                }
            );
        }

        bool find_import_symbol(const std::string& symbol_name, size_t dependency_index, SymbolReference& ref_out) const {
            if (dependency_index >= dependencies_by_name.size()) {
                return false;
            }

            auto find_it = dependency_imports_by_name[dependency_index].find(symbol_name);
            if (find_it == dependency_imports_by_name[dependency_index].end()) {
                return false;
            }

            ref_out.section_index = SectionImport;
            ref_out.symbol_index = find_it->second;
            return true;
        }

        void add_event_symbol(const std::string& symbol_name) {
            // TODO Check if reference_symbols_by_name already contains the name and show a conflict error if so.
            reference_symbols_by_name[symbol_name] = N64Recomp::SymbolReference {
                .section_index = N64Recomp::SectionEvent,
                .symbol_index = event_symbols.size()
            };
            event_symbols.emplace_back(
                N64Recomp::EventSymbol {
                    .base = N64Recomp::ReferenceSymbol {
                        .name = symbol_name,
                        .section_index = N64Recomp::SectionEvent,
                        .section_offset = 0,
                        .is_function = true
                    }
                }
            );
        }

        bool find_event_symbol(const std::string& symbol_name, SymbolReference& ref_out) const {
            SymbolReference ref_found;
            if (!find_reference_symbol(symbol_name, ref_found)) {
                return false;
            }

            // Ignore reference symbols that aren't in the event section.
            if (ref_found.section_index != SectionEvent) {
                return false;
            }

            ref_out = ref_found;
            return true;
        }

        bool add_dependency_event(const std::string& event_name, size_t dependency_index, size_t& dependency_event_index) {
            if (dependency_index >= dependencies_by_name.size()) {
                return false;
            }

            // Prevent adding the same event to a dependency twice. This isn't an error, since a mod could register
            // multiple callbacks to the same event.
            auto find_it = dependency_events_by_name[dependency_index].find(event_name);
            if (find_it != dependency_events_by_name[dependency_index].end()) {
                dependency_event_index = find_it->second;
                return true;
            }

            dependency_event_index = dependency_events.size();
            dependency_events.emplace_back(DependencyEvent{
                .dependency_index = dependency_index,
                .event_name = event_name
            });
            dependency_events_by_name[dependency_index][event_name] = dependency_event_index;
            return true;
        }

        bool add_callback(size_t dependency_event_index, size_t function_index) {
            callbacks.emplace_back(Callback{
                .function_index = function_index,
                .dependency_event_index = dependency_event_index
            });
            return true;
        }

        uint32_t get_reference_section_vram(uint16_t section_index) const {
            if (section_index == N64Recomp::SectionAbsolute) {
                return 0;
            }
            else if (!is_regular_reference_section(section_index)) {
                return 0;
            }
            else {
                return reference_sections[section_index].ram_addr;
            }
        }

        uint32_t get_reference_section_rom(uint16_t section_index) const {
            if (section_index == N64Recomp::SectionAbsolute) {
                return (uint32_t)-1;
            }
            else if (!is_regular_reference_section(section_index)) {
                return (uint32_t)-1;
            }
            else {
                return reference_sections[section_index].rom_addr;
            }
        }

        void copy_reference_sections_from(const Context& rhs) {
            reference_sections = rhs.reference_sections;
        }

        void set_all_reference_sections_relocatable() {
            all_reference_sections_relocatable = true;
        }

    };

    class Generator;
    bool recompile_function(const Context& context, size_t function_index, std::ostream& output_file, std::span<std::vector<uint32_t>> static_funcs, bool tag_reference_relocs);
    bool recompile_function_custom(Generator& generator, const Context& context, size_t function_index, std::ostream& output_file, std::span<std::vector<uint32_t>> static_funcs_out, bool tag_reference_relocs);

    enum class ModSymbolsError {
        Good,
        NotASymbolFile,
        UnknownSymbolFileVersion,
        CorruptSymbolFile,
        FunctionOutOfBounds,
    };

    ModSymbolsError parse_mod_symbols(std::span<const char> data, std::span<const uint8_t> binary, const std::unordered_map<uint32_t, uint16_t>& sections_by_vrom, Context& context_out);
    std::vector<uint8_t> symbols_to_bin_v1(const Context& mod_context);
    
    inline bool is_manual_patch_symbol(uint32_t vram) {
        // Zero-sized symbols between 0x8F000000 and 0x90000000 are manually specified symbols for use with patches.
        // TODO make this configurable or come up with a more sensible solution for dealing with manual symbols for patches.
        return vram >= 0x8F000000 && vram < 0x90000000;
    }

    // Locale-independent ASCII-only version of isalpha.
    inline bool isalpha_nolocale(char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }
    
    // Locale-independent ASCII-only version of isalnum.
    inline bool isalnum_nolocale(char c) {
        return isalpha_nolocale(c) || (c >= '0' && c <= '9');
    }

    inline bool validate_mod_id(std::string_view str) {
        // Disallow empty ids.
        if (str.size() == 0) {
            return false;
        }

        // Allow special dependency ids.
        if (str == N64Recomp::DependencySelf || str == N64Recomp::DependencyBaseRecomp) {
            return true;
        }

        // These following rules basically describe C identifiers. There's no specific reason to enforce them besides colon (currently),
        // so this is just to prevent "weird" mod ids.

        // Check the first character, which must be alphabetical or an underscore.
        if (!isalpha_nolocale(str[0]) && str[0] != '_') {
            return false;
        }

        // Check the remaining characters, which can be alphanumeric or underscore.
        for (char c : str.substr(1)) {
            if (!isalnum_nolocale(c) && c != '_') {
                return false;
            }
        }

        return true;
    }

    inline bool validate_mod_id(const std::string& str) {
        return validate_mod_id(std::string_view{str});
    }
}

#endif
