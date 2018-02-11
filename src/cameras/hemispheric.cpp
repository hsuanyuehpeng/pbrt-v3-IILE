

// cameras/hemispheric.cpp*
#include "cameras/hemispheric.h"
#include "paramset.h"
#include "sampler.h"
#include "stats.h"
#include "filters/gaussian.h"

namespace pbrt {

Float HemisphericCamera::GenerateRay(
        const CameraSample &sample,
        Ray* ray
        ) const {

    ProfilePhase prof(Prof::GenerateCameraRay);
    // Compute environment camera ray direction
    Float theta = Pi * sample.pFilm.y / film->fullResolution.y;
    Float phi = 2 * Pi * sample.pFilm.x / film->fullResolution.x;
    Vector3f dir(std::sin(theta) * std::cos(phi), std::cos(theta),
                 std::sin(theta) * std::sin(phi));
    *ray = Ray(Point3f(0, 0, 0), dir, Infinity,
               Lerp(sample.time, shutterOpen, shutterClose));
    ray->medium = medium;
    *ray = CameraToWorld(*ray);
    return 1;

}

HemisphericCamera* CreateHemisphericCamera(
        int xres,
        int yres,
        const Medium *medium,
        Point3f pos,
        Point3f dir,
        Point2i originalPixel
        ) {

    LOG(INFO) << "CreateHemisphericCamera: pos " << pos;
    LOG(INFO) << "                         dir " << dir;

    // Create lookAt transform
    const Vector3f up (0.f, 0.f, 1.f);
    const Point3f look = Point3f(pos.x+dir.x, pos.y+dir.y, pos.z+dir.z);
    const Transform* cameraTransform = new Transform(LookAt(pos, look, up).GetInverseMatrix());

    AnimatedTransform cam2world (
                cameraTransform,
                0.,
                cameraTransform,
                0.);

    LOG(INFO) << "Creating a HemisphericCamera at position: ["<< pos <<"]";
    LOG(INFO) << "Created a HemisphericCamera with startTransform ["<< *cameraTransform <<"]";

    // Create film
    const Point2i resolution (xres, yres);
    const Bounds2f cropWindow (Point2f(0., 0.), Point2f(1., 1.));
    std::unique_ptr<Filter> filter (new GaussianFilter(Vector2f(2.f, 2.f), 2.f));
    Float scale = 1.;
    Float diagonal = 35.;
    Float maxSampleLuminance = Infinity;
    // TODO change output file name
    Film* film = new Film(resolution, cropWindow, std::move(filter), diagonal,
                          "aux_" + std::to_string(originalPixel.x) + "_" + std::to_string(originalPixel.y) + ".png",
                          scale, maxSampleLuminance);

    Float shutteropen = 0.f;
    Float shutterclose = 1.f;

    return new HemisphericCamera(cam2world, shutteropen, shutterclose,
                                 film, medium);

}

}
