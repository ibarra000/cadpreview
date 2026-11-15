#include "formats/dxf/document.hpp"

#include "common/errors.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <utility>

namespace cadpreview::dxf {

namespace {

constexpr int LAYER_FROZEN = 1; // group 70 bit; see notes/dxf.md#layers

// Reads records (a 0 group plus the groups that follow it) with one group of lookahead.
class RecordReader {
public:
    RecordReader(std::span<const std::byte> data, const ReadLimits& limits)
        : tokenizer_(data), limits_(limits) {}

    std::optional<Record> next() {
        std::optional<Group> start = std::exchange(pending_, std::nullopt);
        if (!start) {
            start = take();
        }
        if (!start) {
            return std::nullopt;
        }
        if (start->code != 0) {
            throw FormatError("DXF record does not start with group 0");
        }
        Record record{start->value, {}};
        while (auto group = take()) {
            if (group->code == 0) {
                pending_ = group;
                break;
            }
            record.groups.push_back(*group);
        }
        return record;
    }

private:
    std::optional<Group> take() {
        auto group = tokenizer_.next();
        if (group && ++groups_ > limits_.maxGroups) {
            throw FormatError("DXF has too many groups");
        }
        return group;
    }

    Tokenizer tokenizer_;
    const ReadLimits& limits_;
    std::optional<Group> pending_;
    std::size_t groups_ = 0;
};

std::string headerVariable(const Record& section, std::string_view name) {
    const auto& groups = section.groups;
    for (std::size_t i = 0; i + 1 < groups.size(); ++i) {
        if (groups[i].code == 9 && groups[i].value == name) {
            return std::string(groups[i + 1].value);
        }
    }
    return {};
}

int hexDigit(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (upper >= 'A' && upper <= 'F') {
        return upper - 'A' + 10;
    }
    throw FormatError("bad hex digit in DXF thumbnail");
}

// see notes/dxf.md#thumbnail
std::vector<std::byte> thumbnailBytes(const Record& section, const ReadLimits& limits) {
    std::vector<std::byte> bytes;
    for (const Group& group : section.groups) {
        if (group.code != 310) {
            continue;
        }
        if (group.value.size() % 2 != 0) {
            throw FormatError("odd-length hex chunk in DXF thumbnail");
        }
        if (bytes.size() + group.value.size() / 2 > limits.maxThumbnailBytes) {
            throw FormatError("DXF thumbnail too large");
        }
        for (std::size_t i = 0; i < group.value.size(); i += 2) {
            const int value = hexDigit(group.value[i]) * 16 + hexDigit(group.value[i + 1]);
            bytes.push_back(static_cast<std::byte>(value));
        }
    }
    return bytes;
}

void readTables(RecordReader& reader, Document& document) {
    while (auto record = reader.next()) {
        if (record->type == "ENDSEC") {
            return;
        }
        if (record->type == "LAYER") {
            const bool frozen = (record->integer(70, 0) & LAYER_FROZEN) != 0;
            const bool off = record->integer(62, 7) < 0;
            if (frozen || off) {
                document.hiddenLayers.insert(upperCase(record->text(2).value_or("")));
            }
        }
    }
}

void countEntity(std::size_t& count, const ReadLimits& limits) {
    if (++count > limits.maxEntities) {
        throw FormatError("DXF has too many entities");
    }
}

void readBlocks(RecordReader& reader, Document& document, const ReadLimits& limits,
                std::size_t& count) {
    std::optional<std::pair<std::string, Block>> current;
    while (auto record = reader.next()) {
        if (record->type == "ENDSEC") {
            return;
        }
        if (record->type == "BLOCK") {
            current.emplace(upperCase(record->text(2).value_or("")),
                            Block{{record->number(10, 0), record->number(20, 0)}, {}});
        } else if (record->type == "ENDBLK") {
            if (current) {
                document.blocks.insert_or_assign(current->first, std::move(current->second));
            }
            current.reset();
        } else if (current) {
            countEntity(count, limits);
            current->second.entities.push_back(std::move(*record));
        }
    }
}

void readEntities(RecordReader& reader, Document& document, const ReadLimits& limits,
                  std::size_t& count) {
    while (auto record = reader.next()) {
        if (record->type == "ENDSEC") {
            return;
        }
        countEntity(count, limits);
        document.entities.push_back(std::move(*record));
    }
}

void skipSection(RecordReader& reader) {
    while (auto record = reader.next()) {
        if (record->type == "ENDSEC") {
            return;
        }
    }
}

} // namespace

std::optional<std::string_view> Record::text(int code) const {
    const auto match = std::ranges::find(groups, code, &Group::code);
    return match != groups.end() ? std::optional<std::string_view>(match->value) : std::nullopt;
}

double Record::number(int code, double fallback) const {
    const auto value = text(code);
    return value ? toDouble(*value).value_or(fallback) : fallback;
}

long long Record::integer(int code, long long fallback) const {
    const auto value = text(code);
    return value ? toInteger(*value).value_or(fallback) : fallback;
}

std::string upperCase(std::string_view text) {
    std::string result(text);
    std::ranges::transform(result, result.begin(),
                           [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return result;
}

Document readDocument(std::span<const std::byte> data, Parts parts, const ReadLimits& limits) {
    Document document;
    RecordReader reader(data, limits);
    std::size_t entities = 0;
    while (auto record = reader.next()) {
        if (record->type == "EOF") {
            break;
        }
        if (record->type != "SECTION") {
            continue;
        }
        const std::string_view name = record->text(2).value_or("");
        const bool everything = parts == Parts::Everything;
        if (name == "HEADER") {
            document.version = headerVariable(*record, "$ACADVER");
            if (parts == Parts::Header) {
                return document;
            }
        } else if (name == "THUMBNAILIMAGE") {
            document.hasThumbnailSection = true;
            document.thumbnail = thumbnailBytes(*record, limits);
        } else if (everything && name == "TABLES") {
            readTables(reader, document);
        } else if (everything && name == "BLOCKS") {
            readBlocks(reader, document, limits, entities);
        } else if (everything && name == "ENTITIES") {
            readEntities(reader, document, limits, entities);
        } else {
            skipSection(reader);
        }
    }
    return document;
}

} // namespace cadpreview::dxf
