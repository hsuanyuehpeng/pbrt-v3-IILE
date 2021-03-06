#ifndef IISPTSCHEDULEMONITOR_H
#define IISPTSCHEDULEMONITOR_H

#include <mutex>
#include "geometry.h"

namespace pbrt {

// ============================================================================
// End points are assumed to be exclusive
struct IisptScheduleMonitorTask
{
    int x0;
    int y0;
    int x1;
    int y1;
    int tilesize;
    int pass;
    int taskNumber;
};

// ============================================================================
class IisptScheduleMonitor
{
private:

    // ------------------------------------------------------------------------
    // Members

    std::mutex mutex;

    // Number of tiles per side in each task
    int NUMBER_TILES = 10;

    // Size of a tile
    float current_radius;

    float update_multiplier;

    // Reset point for samples counter
    int update_interval;

    // Film bounds
    Bounds2i bounds;

    // Current pixels in the film
    int nextx;
    int nexty;

    // Pass number
    int pass = 1;

    // Task number
    int taskNumber = 0;

    // Direct passes
    int nextDirectPass = 0;

public:

    // Constructor ------------------------------------------------------------
    IisptScheduleMonitor(Bounds2i bounds);

    // Public methods ---------------------------------------------------------

    IisptScheduleMonitorTask next_task();

    int getNextDirectPass();

};

} // namespace pbrt

#endif // IISPTSCHEDULEMONITOR_H
