#pragma once

#include <string>
#include <vector>

namespace ArtForge::Presentation {

struct CommandModel {
    std::string id;
    std::string label;
    bool enabled{true};
};

struct StatusModel {
    bool ok{true};
    std::string text;
    std::vector<std::string> diagnostics;
};

struct NavigationTreeNodeModel {
    std::string id;
    std::string label;
    std::vector<NavigationTreeNodeModel> children;
};

struct NavigationTreeModel {
    std::vector<NavigationTreeNodeModel> roots;
};

struct TableColumnModel {
    std::string id;
    std::string label;
};

struct TableRowModel {
    std::string id;
    std::vector<std::string> cells;
};

struct TableModel {
    std::vector<TableColumnModel> columns;
    std::vector<TableRowModel> rows;
};

struct PropertyModel {
    std::string name;
    std::string value;
};

struct PropertyListModel {
    std::string title;
    std::vector<PropertyModel> properties;
};

struct SelectionModel {
    bool hasSelection{};
    std::string sourcePath;
    std::string domain;
    std::string itemType;
    std::string itemId;
    int itemIndex{-1};
    std::string displayLabel;
};

std::string DescribeTableModel(const TableModel& model);
std::string DescribePropertyListModel(const PropertyListModel& model);

}
