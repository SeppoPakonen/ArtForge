#include "ArtForge/History/EventLog.hpp"

namespace ArtForge::History {

std::string_view PersistentEventLogName()
{
    return "ArtForge persistent undo/redo event log";
}

}
