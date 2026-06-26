#include "ArtForge/Presentation/PresentationModels.hpp"

#include <sstream>

namespace ArtForge::Presentation {

std::string DescribeTableModel(const TableModel& model)
{
    std::ostringstream output;
    output << "Columns: " << model.columns.size() << "\n";
    output << "Rows: " << model.rows.size() << "\n";
    for (const auto& row : model.rows) {
        output << "Row: " << row.id;
        for (const auto& cell : row.cells) {
            output << " | " << cell;
        }
        output << "\n";
    }
    return output.str();
}

std::string DescribePropertyListModel(const PropertyListModel& model)
{
    std::ostringstream output;
    output << "Properties: " << model.title << "\n";
    for (const auto& property : model.properties) {
        output << property.name << ": " << property.value << "\n";
    }
    return output.str();
}

}
