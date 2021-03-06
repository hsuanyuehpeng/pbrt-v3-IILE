# ml/main_stdio_net.py

Requires to run python with `-u` flag to turn on binary stdio.

Expected stdin format:

* Intensity raster: 32x32x3 = 3072 float (each 4 bytes)
* Distance raster: 32x32x1 = 1024 float (each 4 bytes)
* Normals raster: 32x32x3 = 3072 float (each 4 bytes)
* Intensity normalization value: 1 float
* Distance normalization value: 1 float

Expected stdout format:

* Intensity raster: 32x32x3 = 3072 float (each 4 bytes)
* Magic characters sequence: 'x' '\n'

## New format

Expected stdin format:

* Intensity raster: 32x32x3 = 3072 float (each 4 bytes)
* Normals raster: 32x32x3 = 3072 float (each 4 bytes)
* Distance raster: 32x32x1 = 1024 float (each 4 bytes)

The order of each raster is a 2D array (height, width)

## ML data loader array format

Each data is a numpy array with shape (channels, height, width), so typically it would be (7, 32, 32).

The channels order is

* Intensity R
* Intensity G
* Intensity B
* Normals X
* Normals Y
* Normals Z
* Distance

The data that the C++ process sends to main_stdio_net is already transformed and normalized and does not require additional processing.

The output of the main_stdio process does not apply the upstream transformations, as those are handled by the C++ process.

## ML data loader augmentations

* 0-3 No flip
* 4-7 Vertical flip
* 8-11 Horizontal flip
* 12-15 Both flip

Each group has 4 indexes for rotations of 0 90 180 270 degrees

## Performance

```
Full time from C++: 51ms/iteration
Full time from C++, optimized writes: 48ms/iteration
Full time from C++, optimized read and writes: 47ms/iteration
NN evaluation time: 27ms/iteration
NN evaluation from/to random numpy arrays: 27ms/iteration
NN evaluation with all transforms: 34ms/iteration
```

```
Before optimization 
time pbrt scene.pbrt --iileDirect=1 --iileIndirect=8 out.exr
real	0m36.624s

After optimization
real	0m30.438s

1 thread
real	0m23.856s

2 threads
real	0m26.131s
```

__Optimizing Transforms__

```
Before optimization caching: 85 examples / second
After optimization caching: 2700 examples / second
```

__Optimizing interpolation vectors to arrays__

Using vectors

```
real	1m0.896s
user	1m34.794s
sys	0m0.437s
gj@gj-ub-4770 ~/git/pbrt-v3-custom-scenes/chairbed1 (master) $ time pbrt --iileIndirect=16 --iileDirect=1 scene.pbrt out.exr
```

Using arrays

```
real	0m59.865s
user	1m33.841s
sys	0m0.320s
```

# Saved images and PBRT internal image representation

In PBRT, images coordiantes X and Y:

* X from left to right
* Y from bottom to top

But the saved images follow

* Y from top to bottom

In the datasets:

* d - uses PBRT exporter
* n - uses custom exporter, Y is reversed
* z - uses custom exporter, Y is reversed
* p - uses PBRT exporter, Y is reversed

The IntensityFilm object handles the Y direction in the same way as PBRT, maintaining consistency with the training dataset. Therefore once an IntensityFilm object is obtained, there is no need to handle manual transformations. Normals and Distance keep their inverted axis, the neural network handles the axes automatically.

# Environment Variables

`IISPT_STDIO_NET_PY_PATH` Location of `main_stdio_net.py` file which contains the python program to evaluate the neural network. Used by PBRT to start the child process. The environment variable is set up by the pbrt launcher.

`IISPT_SCHEDULE_RADIUS_START` Initial radius.

`IISPT_SCHEDULE_RADIUS_RATIO` Radius update multiplier.

`IISPT_SCHEDULE_INTERVAL` Radius interval samples.

`IISPT_RNG_SEED` Initial RNG seed.

`IILE_PATH_SAMPLES_OVERRIDE` Overrides the Path integrator's sampler to use Sobol at the specified samples per pixel

# IISPT Render Algorithm

## Classes

### IisptRenderRunner

One render thread. It includes the main loop logic.

Requires shared objects:

* IISPTIntegrator
* IisptScheduleMonitor
* IisptFilmMonitor (includes sample density information)

It creates its own instance of:

* IISPTdIntegrator
* IisptNnConnector (requires `dcamera` and `scene`)
* RNG

The render loop works as follows

* Obtain current __radius__ from the __ScheduleMonitor__. The ScheduleMonitor updates its internal count automatically
* Use the __RNG__ to generate 2 random pixel samples. Look up the density of the samples and select the one that has lower density
* Obtain camera ray and shoot into scene. If no __intersection__ is found, evaluate infinite lights
* Create __auxCamera__ and use the __dIntegrator__ to render a view
* Use the __NnConnector__ to obtain the predicted intensity
* Set the predicted intensity map on the __auxCamera__
* Create a __filmTile__ in the radius section
* For all pixels within __radius__ and whose intersection and materials are compatible with the original intersection, evaluate __Li__ and update the filmTile
* Send the filmTile to the __filmMonitor__

### IisptScheduleMonitor

Maintains the schedule of influence radius and radius update interval.

The radius schedule uses 2 parameters:

* Initial radius. Defaults to 50, overridden by `IISPT_SCHEDULE_RADIUS_START`
* Update multiplier. Defaults to 0.90, overridden by `IISPT_SCHEDULE_RADIUS_RATIO`

When radius is <= 1, only the original pixel is affected.

The radius update interval is the number of IISPT samples generated after the radius changes. A sample is considered to be generated at each call to __get_current_radius()__.

Defaults to 500, overridden by `IISPT_SCHEDULE_INTERVAL`.

### IisptFilmMonitor

Represents the full rendering film used by IISPT.

All the coordinates in the public API are absolute x and y coordinates, and are converted to internal film indexes automatically.

Holds a 2D array of __IisptPixel__.

__TODO__ This replaces the old IisptFilmMonitor class

Public methods:

* constructor(Bounds2i)
* add_sample(int x, int y, Spectrum s)
* get_density(int x, int y)

### IisptPixel

An IisptPixel has:

* x, y, z color coordinates
* sample_count number of samples obtained at the current location

# Iispt Render Algorithm 2

The new render algorithm uses a regular grid of hemispheric samples, and interpolates between them. The rendering frame is subdivided into smaller rectangular chunks, and each pass will first obtain all the hemispheric samples, and then evaluate all the relevant pixels.

# Training generation

## Multiprocessing control

There are some simple flags that can be used to make it easier to control multiprocessing in reference generation mode.

`IISPT_REFERENCE_CONTROL_MOD` defaults to 1

`IISPT_REFERENCE_CONTROL_MATCH` defaults to 0

The pixel index is modded by the MOD value, and the process will only render the reference pixel if the match value equals.

With the default values, every pixel is rendered.

# NN training

## 01

* per scene normalization
* batch normalization ON
* rprop LR=0.0001

## 02

* per scene normalization
* batch normalization ON
* rprop LR=0.00005

## 03

* mean + standard deviation normalization
* batch normalization ON
* rprop LR=0.00003

## 06

* per frame: log, normalization into [0-1], gamma
* backwards: gamma-1, normalization-1 with saved value, log-1
* comparison level: lowest (gamma corrected level)

Downstream Full (left): log, normalize positive, gamma

Downstream Half (right): Divide by mean, Log, Log

Upstream: InvLog, InvLog, Multiply by mean

Distance Downstream: Add 1, Sqrt, Normalize positive, Gamma

## 07

Similar to 06, but the downstream half use the mean to normalize to 0.1, effectively using 10*mean as ratio.

Use TanH instead of ReLU for many of the layers.

## 08

Changing normal representation to camera-based instead of world-based.

ELU activation function.

Dropout

Paper on ELU: https://arxiv.org/abs/1706.02515

## 09

Convolutional NN

Input data format: Numpy array of shape (depth, height, width)

Input depth is 7: Intensity RGB, Normals RGB, Distance.

```
Channels:
0 R
1 G
2 B
3 n.X
4 n.Y
5 n.Z
6 D
```

Output depth is 3: Intensity RGB

This data type is called ConvNpArray

The corresponding output version with 3 channels is a ConvOutNpArray

# Tiling and interpolation

## New weight

Define the new weight based on closeness in world-coordinates and on normals affinity.

D is the overall distance
P is the normalized position distance
N is the normalized normals distance

```
D = P * N + P
```

When Position is at closest, D is 0.

When Position is at farthest, D is maximal.

```
Weight = max(0, 1-D) + eps
```

Makes sure the weight is positive

When D is 0, weight is 1 + eps

When D is 1, weight is 0 + eps

When D is 1.5, weight is 0 + eps

To compute the normalized position distance:

```
P(a, b) = dist(a, b) / tileDistance
```

To compute a normalized normals distance:

```
N(a, b) = 
    dt = Dot(a, b)
    if dt < 0:
        return 1
    else:
        return 1 - dt
```

When dot product is 1, distance is 0

When dot product is negative, distance is maximal

## New weight 2

Position weight value is computed as the inverse ratio of the distance among the four points

```
wi = (dtt - di) / dtt
```

Where di is one individual distance. dtt is the tile-to-tile minimum distance in world positions. Taken the 4 influence points, calculate the smallest 3D distance among any pair.

Normals weight are computed using the dot product. If the dot product is negative, 0 is used as weight.

The final weight is

```
Weight_i = DistanceWeight_i * NormalWeight_i + eps
```

# GUI

## Directory based controls

`control_gain_XXX` XXX is an integer for the exposure gain. GUI->CPP

`out_indirect.png` Output indirect component

`out_direct.png` Output direct component

`out_combined.png` Output combined component

`info_current_XXX` XXX is the current indirect task being processed

`info_total_XXX` XXX is the total number of indirect tasks

`info_complete` Signals that rendering has finished

## Positional arguments

* 2 PBRT executable path (nodejs version)
* 3 input .pbrt file
* 4 indirect tasks
* 5 direct samples

Example execution

```
node_modules/electron/dist/electron main.js /home/gj/git/pbrt-v3-IISPT/bin/pbrt /home/gj/git/pbrt-v3-scenes-extra/cornell-box/scene.pbrt 16 16
```

# Blender to PBRT

Blender export to OBJ/MTL with Y forward -Z up

```
/home/gj/git/build-pbrt-v3-IISPT-Desktop-Default/obj2pbrt exp.obj exp.pbrt
/home/gj/git/build-pbrt-v3-IISPT-Desktop-Default/pbrt --toply exp.pbrt > scene.pbrt
```

In the scenefile add

```
Integrator "path"
Sampler "sobol" "integer pixelsamples" 1

Scale -1 1 1
Rotate 112 0.725 0.506 -0.467
Translate 0 12 0


Camera "perspective" "float fov" 49

WorldBegin

...

WorldEnd
```

Camera translation

X -> X
Y -> -Y

Blender Y becomes PBRT -Y

Blender Z becomes PBRT Z

Blender export command

```
bpy.ops.export_scene.obj(filepath="/home/gj/git/pbrt-v3-scenes-custom/cbox/cobx.obj", axis_forward="Y", axis_up="-Z", use_materials=True)
```

# P values using kruskal

```
Statistics collection completed
P value L1 gaussian-predicted 4.364805995204919e-189
P value Ss gaussian-predicted 4.8917089088066696e-107
P value L1 low-predicted 0.0
P value Ss low-predicted 1.4126296280156113e-298
```

# TODO

Make more scenes for training and validation

add options for IILE quality in scenefile

GUI: add console output

Make more same-time path vs OSR. See bookmarks in firefox for blendswap interior scenes to be converted.

Direct lighting integrator should use 'one' instead of 'all' lights sampling to scale better to large scenes.

## Portable package requirements

libgconf-2-4

## Selected test scenes

White room daytime

Extra bedroom

Veach ajar