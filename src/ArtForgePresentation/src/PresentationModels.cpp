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

SuggestionCompareModel BuildSuggestionCompareModel(
    std::string targetSummary,
    std::string originalText,
    std::string currentText,
    std::string suggestedText)
{
    SuggestionCompareModel model;
    model.targetSummary = std::move(targetSummary);
    model.originalText = std::move(originalText);
    model.currentText = std::move(currentText);
    model.suggestedText = std::move(suggestedText);
    model.originalAvailable = !model.originalText.empty();
    model.currentAvailable = !model.currentText.empty();
    model.suggestionAvailable = !model.suggestedText.empty();
    model.currentMatchesOriginal = model.originalAvailable && model.currentAvailable && model.originalText == model.currentText;
    model.currentDiffersFromOriginal = model.originalAvailable && model.currentAvailable && model.originalText != model.currentText;
    model.suggestionEqualsCurrent = model.currentAvailable && model.suggestionAvailable && model.currentText == model.suggestedText;

    if (!model.originalAvailable) {
        model.diagnostics.push_back("Original text is unavailable.");
    }
    if (!model.currentAvailable) {
        model.diagnostics.push_back("Current text is unavailable.");
    }
    if (!model.suggestionAvailable) {
        model.diagnostics.push_back("Suggested text is unavailable.");
    }
    if (model.currentDiffersFromOriginal) {
        model.diagnostics.push_back("Current text differs from the original suggestion baseline.");
    }
    if (model.suggestionEqualsCurrent) {
        model.diagnostics.push_back("Suggested text already matches current text.");
    }
    return model;
}

std::string DescribeSuggestionCompareModel(const SuggestionCompareModel& model)
{
    std::ostringstream output;
    output << "Suggestion compare\n";
    output << "Target: " << model.targetSummary << "\n";
    output << "Original available: " << (model.originalAvailable ? "yes" : "no") << "\n";
    output << "Current available: " << (model.currentAvailable ? "yes" : "no") << "\n";
    output << "Suggestion available: " << (model.suggestionAvailable ? "yes" : "no") << "\n";
    output << "Current matches original: " << (model.currentMatchesOriginal ? "yes" : "no") << "\n";
    output << "Current differs from original: " << (model.currentDiffersFromOriginal ? "yes" : "no") << "\n";
    output << "Suggestion equals current: " << (model.suggestionEqualsCurrent ? "yes" : "no") << "\n";
    output << "Original: " << model.originalText << "\n";
    output << "Current: " << model.currentText << "\n";
    output << "Suggested: " << model.suggestedText << "\n";
    for (const auto& diagnostic : model.diagnostics) {
        output << "Diagnostic: " << diagnostic << "\n";
    }
    return output.str();
}

}
