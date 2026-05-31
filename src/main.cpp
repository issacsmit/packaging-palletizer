#define UNICODE
#define _UNICODE

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace cfg {
constexpr int kBinWidth = 4400;       // 440.0 mm, unit: 0.1 mm
constexpr int kBinHeight = 1400;      // 140.0 mm
constexpr int kSideMargin = 50;       // 5.0 mm
constexpr int kGrabGap = 50;          // 5.0 mm between different grabs
constexpr int kMaxOverhang = 200;     // 20.0 mm
constexpr int kPairHeightTolerance = 10; // 1.0 mm
} // namespace cfg

struct Material {
    std::string code;
    std::string name;
    int length = 0;
    int width = 0;
    int height = 0;
};

struct OrderRow {
    int orderSeq = 0;
    int arrivalSeq = 0;
    std::string materialCode;
    std::string materialName;
    int quantity = 0;
};

struct OrderItem {
    int orderSeq = 0;
    int arrivalSeq = 0;
    int expandedIndex = 0;
    Material material;
};

struct PlacedItem {
    int packageId = 0;
    int grabOrder = 0;
    int orderSeq = 0;
    int arrivalSeq = 0;
    int expandedIndex = 0;
    std::string materialCode;
    std::string materialName;
    int grabQuantity = 0;
    int indexInGrab = 0;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct Grab {
    int packageId = 0;
    int grabOrder = 0;
    int orderSeq = 0;
    int quantity = 0;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    std::vector<PlacedItem> items;
};

struct PackagePlan {
    int packageId = 0;
    int orderSeq = 0;
    std::vector<Grab> grabs;
    std::vector<PlacedItem> items;
};

std::wstring utf8ToWide(const std::string& text) {
    if (text.empty()) {
        return L"";
    }
    int count = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring wide(count, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), count);
    return wide;
}

std::string wideToUtf8(const std::wstring& text) {
    if (text.empty()) {
        return "";
    }
    int count = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string utf8(count, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), utf8.data(), count, nullptr, nullptr);
    return utf8;
}

std::wstring getExeDir() {
    wchar_t buffer[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        return L".";
    }
    std::wstring path(buffer, len);
    size_t pos = path.find_last_of(L"\\/");
    return pos == std::wstring::npos ? L"." : path.substr(0, pos);
}

bool fileExists(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

std::string readTextFile(const std::wstring& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("cannot open file: " + wideToUtf8(path));
    }
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size)) {
        CloseHandle(file);
        throw std::runtime_error("cannot get file size: " + wideToUtf8(path));
    }
    if (size.QuadPart > 50LL * 1024 * 1024) {
        CloseHandle(file);
        throw std::runtime_error("file is too large: " + wideToUtf8(path));
    }
    std::string data(static_cast<size_t>(size.QuadPart), '\0');
    DWORD read = 0;
    if (!data.empty() && !ReadFile(file, data.data(), static_cast<DWORD>(data.size()), &read, nullptr)) {
        CloseHandle(file);
        throw std::runtime_error("cannot read file: " + wideToUtf8(path));
    }
    CloseHandle(file);
    data.resize(read);
    if (data.size() >= 3 && static_cast<unsigned char>(data[0]) == 0xEF &&
        static_cast<unsigned char>(data[1]) == 0xBB && static_cast<unsigned char>(data[2]) == 0xBF) {
        data.erase(0, 3);
    }
    return data;
}

void ensureDirectory(const std::wstring& path) {
    if (CreateDirectoryW(path.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS) {
        return;
    }
    throw std::runtime_error("cannot create directory: " + wideToUtf8(path));
}

void writeTextFile(const std::wstring& path, const std::string& content, bool bom = true) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("cannot write file: " + wideToUtf8(path));
    }
    DWORD written = 0;
    if (bom) {
        const unsigned char prefix[3] = {0xEF, 0xBB, 0xBF};
        WriteFile(file, prefix, 3, &written, nullptr);
    }
    if (!content.empty()) {
        if (!WriteFile(file, content.data(), static_cast<DWORD>(content.size()), &written, nullptr)) {
            CloseHandle(file);
            throw std::runtime_error("write failed: " + wideToUtf8(path));
        }
    }
    CloseHandle(file);
}

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::string current;
    for (char ch : text) {
        if (ch == '\n') {
            if (!current.empty() && current.back() == '\r') {
                current.pop_back();
            }
            lines.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    if (!current.empty()) {
        if (!current.empty() && current.back() == '\r') {
            current.pop_back();
        }
        lines.push_back(current);
    }
    return lines;
}

std::vector<std::string> parseCsvLine(const std::string& line) {
    std::vector<std::string> fields;
    std::string cell;
    bool quoted = false;
    for (size_t i = 0; i < line.size(); ++i) {
        char ch = line[i];
        if (quoted) {
            if (ch == '"' && i + 1 < line.size() && line[i + 1] == '"') {
                cell.push_back('"');
                ++i;
            } else if (ch == '"') {
                quoted = false;
            } else {
                cell.push_back(ch);
            }
        } else {
            if (ch == '"') {
                quoted = true;
            } else if (ch == ',') {
                fields.push_back(cell);
                cell.clear();
            } else {
                cell.push_back(ch);
            }
        }
    }
    fields.push_back(cell);
    return fields;
}

int toInt(const std::string& value, const std::string& fieldName) {
    try {
        size_t pos = 0;
        int parsed = std::stoi(value, &pos);
        if (pos != value.size()) {
            throw std::invalid_argument("tail");
        }
        return parsed;
    } catch (...) {
        throw std::runtime_error("invalid integer in " + fieldName + ": " + value);
    }
}

std::string csvEscape(const std::string& value) {
    bool needQuote = value.find_first_of(",\"\r\n") != std::string::npos;
    std::string out;
    for (char ch : value) {
        if (ch == '"') {
            out += "\"\"";
        } else {
            out.push_back(ch);
        }
    }
    return needQuote ? "\"" + out + "\"" : out;
}

std::string mm(int tenths) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << (tenths / 10.0);
    return oss.str();
}

class DataLoader {
public:
    explicit DataLoader(std::wstring dataDir) : dataDir_(std::move(dataDir)) {}

    void load() {
        loadMaterials();
        loadOrders();
        expandOrders();
    }

    const std::unordered_map<std::string, Material>& materials() const { return materials_; }
    const std::vector<OrderRow>& orderRows() const { return orderRows_; }
    const std::map<int, std::vector<OrderItem>>& orders() const { return orders_; }

private:
    std::wstring dataDir_;
    std::unordered_map<std::string, Material> materials_;
    std::vector<OrderRow> orderRows_;
    std::map<int, std::vector<OrderItem>> orders_;

    std::wstring dataPath(const wchar_t* name) const {
        return dataDir_ + L"\\" + name;
    }

    void loadMaterials() {
        std::string text = readTextFile(dataPath(L"materials.csv"));
        auto lines = splitLines(text);
        if (lines.size() < 2) {
            throw std::runtime_error("materials.csv has no data rows");
        }
        for (size_t i = 1; i < lines.size(); ++i) {
            if (lines[i].empty()) {
                continue;
            }
            auto f = parseCsvLine(lines[i]);
            if (f.size() < 5) {
                throw std::runtime_error("materials.csv row has fewer than 5 columns");
            }
            Material m;
            m.code = f[0];
            m.name = f[1];
            m.length = toInt(f[2], "length");
            m.width = toInt(f[3], "width");
            m.height = toInt(f[4], "height");
            if (m.width <= 0 || m.height <= 0 || m.width > cfg::kBinWidth || m.height > cfg::kBinHeight) {
                throw std::runtime_error("invalid material size: " + m.name);
            }
            materials_[m.code] = m;
        }
    }

    void loadOrders() {
        std::string text = readTextFile(dataPath(L"orders.csv"));
        auto lines = splitLines(text);
        if (lines.size() < 2) {
            throw std::runtime_error("orders.csv has no data rows");
        }
        for (size_t i = 1; i < lines.size(); ++i) {
            if (lines[i].empty()) {
                continue;
            }
            auto f = parseCsvLine(lines[i]);
            if (f.size() < 5) {
                throw std::runtime_error("orders.csv row has fewer than 5 columns");
            }
            OrderRow row;
            row.orderSeq = toInt(f[0], "orderSeq");
            row.arrivalSeq = toInt(f[1], "arrivalSeq");
            row.materialCode = f[2];
            row.materialName = f[3];
            row.quantity = toInt(f[4], "quantity");
            if (row.quantity <= 0) {
                throw std::runtime_error("order quantity must be positive");
            }
            if (materials_.find(row.materialCode) == materials_.end()) {
                throw std::runtime_error("material not found for order row: " + row.materialCode);
            }
            orderRows_.push_back(row);
        }
    }

    void expandOrders() {
        int expanded = 0;
        for (const auto& row : orderRows_) {
            const Material& material = materials_.at(row.materialCode);
            for (int i = 0; i < row.quantity; ++i) {
                OrderItem item;
                item.orderSeq = row.orderSeq;
                item.arrivalSeq = row.arrivalSeq;
                item.expandedIndex = ++expanded;
                item.material = material;
                orders_[item.orderSeq].push_back(item);
            }
        }
    }
};

class Geometry {
public:
    static bool overlap1d(int a0, int a1, int b0, int b1) {
        return a0 < b1 && b0 < a1;
    }

    static bool sameGrab(const PlacedItem& a, const PlacedItem& b) {
        return a.packageId == b.packageId && a.grabOrder == b.grabOrder;
    }

    static int maxUnsupportedWidth(const PlacedItem& candidate, const std::vector<PlacedItem>& placed) {
        if (candidate.y == 0) {
            return 0;
        }
        std::vector<std::pair<int, int>> supported;
        for (const auto& item : placed) {
            if (item.y + item.height != candidate.y) {
                continue;
            }
            int left = std::max(candidate.x, item.x);
            int right = std::min(candidate.x + candidate.width, item.x + item.width);
            if (left < right) {
                supported.emplace_back(left, right);
            }
        }
        if (supported.empty()) {
            return candidate.width;
        }
        std::sort(supported.begin(), supported.end());
        std::vector<std::pair<int, int>> merged;
        for (auto segment : supported) {
            if (merged.empty() || segment.first > merged.back().second) {
                merged.push_back(segment);
            } else {
                merged.back().second = std::max(merged.back().second, segment.second);
            }
        }
        int maxGap = 0;
        int cursor = candidate.x;
        for (auto segment : merged) {
            maxGap = std::max(maxGap, segment.first - cursor);
            cursor = std::max(cursor, segment.second);
        }
        maxGap = std::max(maxGap, candidate.x + candidate.width - cursor);
        return maxGap;
    }
};

class Palletizer {
public:
    std::vector<PackagePlan> build(const std::map<int, std::vector<OrderItem>>& orders) {
        std::vector<PackagePlan> result;
        int nextPackageId = 1;

        for (const auto& [orderSeq, items] : orders) {
            PackagePlan current;
            current.packageId = nextPackageId;
            current.orderSeq = orderSeq;
            size_t index = 0;

            while (index < items.size()) {
                bool placed = false;
                std::vector<int> choices;
                if (canPair(items, index)) {
                    choices.push_back(2);
                }
                choices.push_back(1);

                for (int quantity : choices) {
                    Grab grab;
                    if (findPlacement(current, items, index, quantity, grab)) {
                        grab.packageId = current.packageId;
                        grab.grabOrder = static_cast<int>(current.grabs.size()) + 1;
                        grab.orderSeq = orderSeq;
                        for (auto& item : grab.items) {
                            item.packageId = grab.packageId;
                            item.grabOrder = grab.grabOrder;
                        }
                        current.grabs.push_back(grab);
                        for (const auto& placedItem : grab.items) {
                            current.items.push_back(placedItem);
                        }
                        index += quantity;
                        placed = true;
                        break;
                    }
                }

                if (!placed) {
                    if (current.items.empty()) {
                        throw std::runtime_error("single item cannot fit into an empty package: " + items[index].material.name);
                    }
                    result.push_back(current);
                    current = PackagePlan{};
                    current.packageId = ++nextPackageId;
                    current.orderSeq = orderSeq;
                }
            }

            if (!current.items.empty()) {
                result.push_back(current);
                ++nextPackageId;
            }
        }
        return result;
    }

private:
    static bool canPair(const std::vector<OrderItem>& items, size_t index) {
        if (index + 1 >= items.size()) {
            return false;
        }
        return std::abs(items[index].material.height - items[index + 1].material.height) <= cfg::kPairHeightTolerance;
    }

    bool findPlacement(const PackagePlan& package,
                       const std::vector<OrderItem>& items,
                       size_t index,
                       int quantity,
                       Grab& out) const {
        if (quantity == 2 && !canPair(items, index)) {
            return false;
        }

        int width = 0;
        int height = 0;
        for (int i = 0; i < quantity; ++i) {
            width += items[index + i].material.width;
            height = std::max(height, items[index + i].material.height);
        }
        if (width > cfg::kBinWidth - 2 * cfg::kSideMargin || height > cfg::kBinHeight) {
            return false;
        }

        auto candidatesY = yCandidates(package, height);
        auto candidatesX = xCandidates(package, width);

        for (int y : candidatesY) {
            for (int x : candidatesX) {
                Grab candidate = makeGrab(items, index, quantity, x, y, width, height);
                if (fits(package, candidate)) {
                    out = candidate;
                    return true;
                }
            }
        }
        return false;
    }

    static std::vector<int> yCandidates(const PackagePlan& package, int groupHeight) {
        std::set<int> values;
        values.insert(0);
        for (const auto& item : package.items) {
            int top = item.y + item.height;
            if (top + groupHeight <= cfg::kBinHeight) {
                values.insert(top);
            }
        }
        return std::vector<int>(values.begin(), values.end());
    }

    static void addCandidateX(std::set<int>& values, int x, int groupWidth) {
        if (x >= cfg::kSideMargin && x + groupWidth <= cfg::kBinWidth - cfg::kSideMargin) {
            values.insert(x);
        }
    }

    static std::vector<int> xCandidates(const PackagePlan& package, int groupWidth) {
        std::set<int> values;
        addCandidateX(values, cfg::kSideMargin, groupWidth);
        addCandidateX(values, cfg::kBinWidth - cfg::kSideMargin - groupWidth, groupWidth);
        for (const auto& item : package.items) {
            addCandidateX(values, item.x, groupWidth);
            addCandidateX(values, item.x + item.width - groupWidth, groupWidth);
            addCandidateX(values, item.x - cfg::kMaxOverhang, groupWidth);
            addCandidateX(values, item.x + item.width - groupWidth + cfg::kMaxOverhang, groupWidth);
            addCandidateX(values, item.x + item.width + cfg::kGrabGap, groupWidth);
            addCandidateX(values, item.x - groupWidth - cfg::kGrabGap, groupWidth);
        }
        return std::vector<int>(values.begin(), values.end());
    }

    static Grab makeGrab(const std::vector<OrderItem>& items,
                         size_t index,
                         int quantity,
                         int x,
                         int y,
                         int width,
                         int height) {
        Grab grab;
        grab.quantity = quantity;
        grab.x = x;
        grab.y = y;
        grab.width = width;
        grab.height = height;
        int cursor = x;
        for (int i = 0; i < quantity; ++i) {
            const auto& src = items[index + i];
            PlacedItem placed;
            placed.orderSeq = src.orderSeq;
            placed.arrivalSeq = src.arrivalSeq;
            placed.expandedIndex = src.expandedIndex;
            placed.materialCode = src.material.code;
            placed.materialName = src.material.name;
            placed.grabQuantity = quantity;
            placed.indexInGrab = i + 1;
            placed.x = cursor;
            placed.y = y;
            placed.width = src.material.width;
            placed.height = src.material.height;
            grab.items.push_back(placed);
            cursor += src.material.width;
        }
        return grab;
    }

    static bool fits(const PackagePlan& package, const Grab& candidate) {
        if (candidate.x < cfg::kSideMargin || candidate.x + candidate.width > cfg::kBinWidth - cfg::kSideMargin) {
            return false;
        }
        if (candidate.y < 0 || candidate.y + candidate.height > cfg::kBinHeight) {
            return false;
        }

        for (const auto& newItem : candidate.items) {
            for (const auto& oldItem : package.items) {
                bool yOverlap = Geometry::overlap1d(newItem.y, newItem.y + newItem.height,
                                                    oldItem.y, oldItem.y + oldItem.height);
                if (!yOverlap) {
                    continue;
                }
                bool xOverlap = Geometry::overlap1d(newItem.x, newItem.x + newItem.width,
                                                    oldItem.x, oldItem.x + oldItem.width);
                if (xOverlap) {
                    return false;
                }
                int gap = newItem.x < oldItem.x ? oldItem.x - (newItem.x + newItem.width)
                                                 : newItem.x - (oldItem.x + oldItem.width);
                if (gap < cfg::kGrabGap) {
                    return false;
                }
            }
        }

        for (const auto& newItem : candidate.items) {
            if (Geometry::maxUnsupportedWidth(newItem, package.items) > cfg::kMaxOverhang) {
                return false;
            }
        }
        return true;
    }
};

class Validator {
public:
    std::vector<std::string> validate(const std::vector<PackagePlan>& packages) const {
        std::vector<std::string> errors;
        for (const auto& package : packages) {
            validatePackage(package, errors);
        }
        return errors;
    }

private:
    static void validatePackage(const PackagePlan& package, std::vector<std::string>& errors) {
        std::set<int> orders;
        for (const auto& item : package.items) {
            orders.insert(item.orderSeq);
            if (item.x < cfg::kSideMargin || item.x + item.width > cfg::kBinWidth - cfg::kSideMargin ||
                item.y < 0 || item.y + item.height > cfg::kBinHeight) {
                errors.push_back("package " + std::to_string(package.packageId) + " has out-of-bound item");
            }
            if (Geometry::maxUnsupportedWidth(item, package.items) > cfg::kMaxOverhang) {
                errors.push_back("package " + std::to_string(package.packageId) + " has overhang greater than 20mm");
            }
        }
        if (orders.size() > 1) {
            errors.push_back("package " + std::to_string(package.packageId) + " mixes different orders");
        }
        for (size_t i = 0; i < package.items.size(); ++i) {
            for (size_t j = i + 1; j < package.items.size(); ++j) {
                const auto& a = package.items[i];
                const auto& b = package.items[j];
                bool yOverlap = Geometry::overlap1d(a.y, a.y + a.height, b.y, b.y + b.height);
                bool xOverlap = Geometry::overlap1d(a.x, a.x + a.width, b.x, b.x + b.width);
                if (xOverlap && yOverlap) {
                    errors.push_back("package " + std::to_string(package.packageId) + " has overlapped items");
                }
                if (!Geometry::sameGrab(a, b) && yOverlap && !xOverlap) {
                    int gap = a.x < b.x ? b.x - (a.x + a.width) : a.x - (b.x + b.width);
                    if (gap < cfg::kGrabGap) {
                        errors.push_back("package " + std::to_string(package.packageId) + " has grab gap less than 5mm");
                    }
                }
            }
        }
        for (const auto& grab : package.grabs) {
            if (grab.quantity != static_cast<int>(grab.items.size()) || (grab.quantity != 1 && grab.quantity != 2)) {
                errors.push_back("package " + std::to_string(package.packageId) + " has invalid grab quantity");
            }
            if (grab.quantity == 2) {
                const auto& a = grab.items[0];
                const auto& b = grab.items[1];
                if (std::abs(a.height - b.height) > cfg::kPairHeightTolerance) {
                    errors.push_back("package " + std::to_string(package.packageId) + " has invalid two-item grab height");
                }
                if (a.x + a.width != b.x || a.y != b.y) {
                    errors.push_back("package " + std::to_string(package.packageId) + " has invalid two-item grab placement");
                }
            }
        }
    }
};

class ResultWriter {
public:
    explicit ResultWriter(std::wstring outputDir) : outputDir_(std::move(outputDir)) {}

    void writeAll(const std::vector<PackagePlan>& packages,
                  double elapsedMs,
                  int materialCount,
                  int sourceRows,
                  int expandedCount,
                  const std::vector<std::string>& validationErrors) const {
        ensureDirectory(outputDir_);
        writeStackingResult(packages);
        writePackageSummary(packages);
        writeStats(packages, elapsedMs, materialCount, sourceRows, expandedCount, validationErrors);
    }

private:
    std::wstring outputDir_;

    std::wstring path(const wchar_t* name) const {
        return outputDir_ + L"\\" + name;
    }

    static int totalGrabs(const std::vector<PackagePlan>& packages) {
        int count = 0;
        for (const auto& p : packages) {
            count += static_cast<int>(p.grabs.size());
        }
        return count;
    }

    static int totalItems(const PackagePlan& package) {
        return static_cast<int>(package.items.size());
    }

    static int usedArea(const PackagePlan& package) {
        int area = 0;
        for (const auto& item : package.items) {
            area += item.width * item.height;
        }
        return area;
    }

    void writeStackingResult(const std::vector<PackagePlan>& packages) const {
        std::ostringstream out;
        out << "码垛号,码垛机械手抓取顺序号,订单顺序号,来料顺序号,物料编号,卷烟名称,抓取数量,组内序号,"
               "X坐标(0.1mm),Y坐标(0.1mm),宽(0.1mm),高(0.1mm),X坐标(mm),Y坐标(mm),宽(mm),高(mm)\n";
        for (const auto& package : packages) {
            for (const auto& grab : package.grabs) {
                for (const auto& item : grab.items) {
                    out << package.packageId << ','
                        << grab.grabOrder << ','
                        << item.orderSeq << ','
                        << item.arrivalSeq << ','
                        << csvEscape(item.materialCode) << ','
                        << csvEscape(item.materialName) << ','
                        << grab.quantity << ','
                        << item.indexInGrab << ','
                        << item.x << ','
                        << item.y << ','
                        << item.width << ','
                        << item.height << ','
                        << mm(item.x) << ','
                        << mm(item.y) << ','
                        << mm(item.width) << ','
                        << mm(item.height) << '\n';
                }
            }
        }
        writeTextFile(path(L"stacking_result.csv"), out.str());
    }

    void writePackageSummary(const std::vector<PackagePlan>& packages) const {
        std::ostringstream out;
        out << "码垛号,订单顺序号,条烟数量,机械手抓取次数,面积利用率\n";
        const double binArea = static_cast<double>(cfg::kBinWidth * cfg::kBinHeight);
        for (const auto& package : packages) {
            double utilization = usedArea(package) / binArea;
            out << package.packageId << ','
                << package.orderSeq << ','
                << totalItems(package) << ','
                << package.grabs.size() << ','
                << std::fixed << std::setprecision(4) << utilization << '\n';
        }
        writeTextFile(path(L"package_summary.csv"), out.str());
    }

    void writeStats(const std::vector<PackagePlan>& packages,
                    double elapsedMs,
                    int materialCount,
                    int sourceRows,
                    int expandedCount,
                    const std::vector<std::string>& validationErrors) const {
        std::ostringstream out;
        out << "包装码垛算法运行统计\n";
        out << "物料数量: " << materialCount << '\n';
        out << "来料数据行数: " << sourceRows << '\n';
        out << "展开后条烟数量: " << expandedCount << '\n';
        out << "码垛数量: " << packages.size() << '\n';
        out << "机械手抓取次数: " << totalGrabs(packages) << '\n';
        out << "运行时间(ms): " << std::fixed << std::setprecision(3) << elapsedMs << '\n';
        out << "校验结果: " << (validationErrors.empty() ? "通过" : "未通过") << '\n';
        for (const auto& error : validationErrors) {
            out << "- " << error << '\n';
        }
        writeTextFile(path(L"run_stats.txt"), out.str());
    }
};

struct DrawContext {
    const PackagePlan* package = nullptr;
};

class Win32Visualizer {
public:
    static void show(const PackagePlan& package) {
        DrawContext ctx;
        ctx.package = &package;

        HINSTANCE instance = GetModuleHandleW(nullptr);
        const wchar_t* className = L"PalletizerVisualizerWindow";
        WNDCLASSW wc{};
        wc.lpfnWndProc = &Win32Visualizer::windowProc;
        wc.hInstance = instance;
        wc.lpszClassName = className;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        RegisterClassW(&wc);

        std::wstring title = L"码垛垛型图 - #" + std::to_wstring(package.packageId);
        HWND hwnd = CreateWindowExW(0, className, title.c_str(), WS_OVERLAPPEDWINDOW,
                                    CW_USEDEFAULT, CW_USEDEFAULT, 1120, 620,
                                    nullptr, nullptr, instance, &ctx);
        if (!hwnd) {
            std::cerr << "Cannot create visualization window.\n";
            return;
        }
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);

        MSG msg{};
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (!IsWindow(hwnd)) {
                break;
            }
        }
    }

private:
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (msg == WM_NCCREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        }
        auto* ctx = reinterpret_cast<DrawContext*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        switch (msg) {
        case WM_PAINT:
            if (ctx && ctx->package) {
                draw(hwnd, *ctx->package);
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
    }

    static COLORREF colorForGrab(int grabOrder) {
        static const COLORREF colors[] = {
            RGB(130, 205, 255), RGB(255, 210, 120), RGB(145, 220, 150), RGB(190, 170, 255),
            RGB(255, 145, 145), RGB(165, 230, 220), RGB(235, 185, 245), RGB(215, 230, 130),
            RGB(180, 205, 255), RGB(255, 180, 115)
        };
        return colors[(grabOrder - 1) % (sizeof(colors) / sizeof(colors[0]))];
    }

    static void draw(HWND hwnd, const PackagePlan& package) {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT client{};
        GetClientRect(hwnd, &client);

        HDC mem = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, client.right - client.left, client.bottom - client.top);
        HGDIOBJ oldBmp = SelectObject(mem, bmp);

        HBRUSH bg = CreateSolidBrush(RGB(250, 249, 232));
        FillRect(mem, &client, bg);
        DeleteObject(bg);

        HFONT titleFont = CreateFontW(26, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
        HFONT itemFont = CreateFontW(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
        SelectObject(mem, titleFont);
        SetBkMode(mem, TRANSPARENT);
        SetTextColor(mem, RGB(30, 30, 30));

        std::wstring title = L"码垛号 " + std::to_wstring(package.packageId) +
                             L"    订单 " + std::to_wstring(package.orderSeq) +
                             L"    条烟 " + std::to_wstring(package.items.size()) +
                             L"    抓取 " + std::to_wstring(package.grabs.size());
        TextOutW(mem, 28, 18, title.c_str(), static_cast<int>(title.size()));

        const int left = 36;
        const int top = 76;
        const int rightPad = 36;
        const int bottomPad = 48;
        int drawW = std::max(100, static_cast<int>(client.right) - left - rightPad);
        int drawH = std::max(100, static_cast<int>(client.bottom) - top - bottomPad);
        double scale = std::min(drawW / static_cast<double>(cfg::kBinWidth),
                                drawH / static_cast<double>(cfg::kBinHeight));
        int boxW = static_cast<int>(cfg::kBinWidth * scale);
        int boxH = static_cast<int>(cfg::kBinHeight * scale);
        int originX = left;
        int originY = top + boxH;

        HPEN borderPen = CreatePen(PS_SOLID, 2, RGB(20, 75, 170));
        HGDIOBJ oldPen = SelectObject(mem, borderPen);
        HBRUSH emptyBrush = reinterpret_cast<HBRUSH>(GetStockObject(HOLLOW_BRUSH));
        HGDIOBJ oldBrush = SelectObject(mem, emptyBrush);
        Rectangle(mem, originX, originY - boxH, originX + boxW, originY);

        HPEN marginPen = CreatePen(PS_DOT, 1, RGB(120, 120, 120));
        SelectObject(mem, marginPen);
        int marginPx = static_cast<int>(cfg::kSideMargin * scale);
        MoveToEx(mem, originX + marginPx, originY - boxH, nullptr);
        LineTo(mem, originX + marginPx, originY);
        MoveToEx(mem, originX + boxW - marginPx, originY - boxH, nullptr);
        LineTo(mem, originX + boxW - marginPx, originY);

        SelectObject(mem, itemFont);
        for (const auto& item : package.items) {
            int x0 = originX + static_cast<int>(item.x * scale);
            int y0 = originY - static_cast<int>((item.y + item.height) * scale);
            int x1 = originX + static_cast<int>((item.x + item.width) * scale);
            int y1 = originY - static_cast<int>(item.y * scale);

            HBRUSH brush = CreateSolidBrush(colorForGrab(item.grabOrder));
            HPEN pen = CreatePen(PS_SOLID, 2, RGB(25, 75, 185));
            SelectObject(mem, brush);
            SelectObject(mem, pen);
            Rectangle(mem, x0, y0, x1, y1);
            DeleteObject(brush);
            DeleteObject(pen);

            RECT textRect{x0 + 3, y0 + 3, x1 - 3, y1 - 3};
            std::wstring label = std::to_wstring(item.grabOrder) + L"." + utf8ToWide(item.materialName);
            SetTextColor(mem, RGB(15, 15, 15));
            DrawTextW(mem, label.c_str(), static_cast<int>(label.size()), &textRect,
                      DT_CENTER | DT_VCENTER | DT_WORDBREAK | DT_END_ELLIPSIS);
        }

        SelectObject(mem, titleFont);
        std::wstring note = L"比例绘制：宽 440mm，高 140mm；虚线为左右 5mm 边距";
        TextOutW(mem, originX, originY + 14, note.c_str(), static_cast<int>(note.size()));

        BitBlt(hdc, 0, 0, client.right - client.left, client.bottom - client.top, mem, 0, 0, SRCCOPY);

        SelectObject(mem, oldBrush);
        SelectObject(mem, oldPen);
        SelectObject(mem, oldBmp);
        DeleteObject(marginPen);
        DeleteObject(borderPen);
        DeleteObject(titleFont);
        DeleteObject(itemFont);
        DeleteObject(bmp);
        DeleteDC(mem);
        EndPaint(hwnd, &ps);
    }
};

std::wstring resolveDataDir() {
    std::wstring exeDir = getExeDir();
    std::vector<std::wstring> candidates = {
        L"data",
        exeDir + L"\\data",
        exeDir + L"\\..\\data",
        exeDir + L"\\..\\执行文件及资源包\\data"
    };
    for (const auto& dir : candidates) {
        if (fileExists(dir + L"\\materials.csv") && fileExists(dir + L"\\orders.csv")) {
            return dir;
        }
    }
    throw std::runtime_error("cannot find data/materials.csv and data/orders.csv");
}

std::wstring resolveOutputDir(const std::wstring& dataDir) {
    std::wstring normalized = dataDir;
    std::replace(normalized.begin(), normalized.end(), L'/', L'\\');
    if (normalized.size() >= 5 && normalized.substr(normalized.size() - 5) == L"\\data") {
        return normalized.substr(0, normalized.size() - 5) + L"\\output";
    }
    if (normalized == L"data") {
        return L"output";
    }
    return getExeDir() + L"\\output";
}

int expandedItemCount(const std::map<int, std::vector<OrderItem>>& orders) {
    int total = 0;
    for (const auto& [_, items] : orders) {
        total += static_cast<int>(items.size());
    }
    return total;
}

int grabCount(const std::vector<PackagePlan>& packages) {
    int total = 0;
    for (const auto& package : packages) {
        total += static_cast<int>(package.grabs.size());
    }
    return total;
}

const PackagePlan* findPackage(const std::vector<PackagePlan>& packages, int id) {
    for (const auto& package : packages) {
        if (package.packageId == id) {
            return &package;
        }
    }
    return nullptr;
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    try {
        std::wstring exeDir = getExeDir();
        std::wstring dataDir = resolveDataDir();
        std::wstring outputDir = resolveOutputDir(dataDir);

        auto t0 = std::chrono::steady_clock::now();
        DataLoader loader(dataDir);
        loader.load();
        Palletizer palletizer;
        auto packages = palletizer.build(loader.orders());
        Validator validator;
        auto errors = validator.validate(packages);
        auto t1 = std::chrono::steady_clock::now();
        double elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

        ResultWriter writer(outputDir);
        writer.writeAll(packages, elapsedMs,
                        static_cast<int>(loader.materials().size()),
                        static_cast<int>(loader.orderRows().size()),
                        expandedItemCount(loader.orders()),
                        errors);

        std::cout << "包装码垛计算完成\n";
        std::cout << "数据目录: " << wideToUtf8(dataDir) << "\n";
        std::cout << "输出目录: " << wideToUtf8(outputDir) << "\n";
        std::cout << "物料数量: " << loader.materials().size() << "\n";
        std::cout << "来料行数: " << loader.orderRows().size() << "\n";
        std::cout << "展开条烟: " << expandedItemCount(loader.orders()) << "\n";
        std::cout << "码垛数量: " << packages.size() << "\n";
        std::cout << "机械手抓取次数: " << grabCount(packages) << "\n";
        std::cout << "运行时间(ms): " << std::fixed << std::setprecision(3) << elapsedMs << "\n";
        std::cout << "校验结果: " << (errors.empty() ? "通过" : "未通过") << "\n";
        if (!errors.empty()) {
            for (const auto& error : errors) {
                std::cout << "- " << error << "\n";
            }
        }

        while (true) {
            std::cout << "\n请输入要显示的码垛号(1-" << packages.size() << "，输入0退出): ";
            int id = 0;
            if (!(std::cin >> id)) {
                break;
            }
            if (id == 0) {
                break;
            }
            const PackagePlan* package = findPackage(packages, id);
            if (!package) {
                std::cout << "码垛号不存在。\n";
                continue;
            }
            Win32Visualizer::show(*package);
        }
        return errors.empty() ? 0 : 2;
    } catch (const std::exception& ex) {
        std::cerr << "程序错误: " << ex.what() << "\n";
        return 1;
    }
}
