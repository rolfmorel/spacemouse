# spacemouse-utils
A collection of simple utilies for 3D/6DoF input devices.

Simple programs for:
* retrieving information about connected 6DoF devices
* retrieving and setting/switching the LED state of connected 6DoF devices
* recieving events on connected 6DoF devices (device, motion and button events)
* testing/debugging (of libspacemouse)

### Currently supported devices
3Dconnexion USB human interface devices:
* SpaceExplorer
* SpaceNavigator
* etc.

## Usage

    $ spacemouse-list
    devnode: /dev/input/event4
    manufacturer: 3Dconnexion
    product: SpaceNavigator

    devnode: /dev/input/event0
    manufacturer: 3Dconnexion
    product: SpaceExplorer

- - - - -
    $ spacemouse-led
    /dev/input/event4: off
    /dev/input/event0: off
- - - - -
    $ spacemouse-led on
- - - - -
    $ spacemouse-led -d /dev/input/event4 switch
    /dev/input/event4: switched off
- - - - -
    $ spacemouse-event
    motion: forward
    motion: forward
    motion: right
    motion: down
    motion: up
    button: 1 press
    button: 1 release
    button: 0 press
    button: 0 release
    device: /dev/input/event4 3Dconnexion SpaceNavigator disconnect
    device: /dev/input/event4 3Dconnexion SpaceNavigator connect
    button: 0 press
    button: 0 release
    ...
- - - - -
    $ spacemouse-test
    device id: 1
      devnode: /dev/input/event4
      manufacturer: 3Dconnexion
      product: SpaceNavigator
    device id: 2
      devnode: /dev/input/event0
      manufacturer: 3Dconnexion
      product: SpaceExplorer
    device id 1: got motion event: t(-1, -19, 0) r(0, 0, 0) period(0)
    device id 1: got motion event: t(-7, 0, 10) r(0, 0, 0) period(16)
    device id 1: got motion event: t(-7, 0, 10) r(-29, 0, -14) period(8)
    device id 1: got motion event: t(0, -129, 0) r(-29, 0, -14) period(8)
    device id 1: got motion event: t(0, -129, 0) r(-37, 0, -111) period(8)
    device id 1: got motion event: t(-33, -44, 41) r(-37, 0, -111) period(8)
    device id 1: got motion event: t(-33, -44, 41) r(-81, 0, -109) period(8)
    device id 1: got motion event: t(-12, -125, 60) r(-81, 0, -109) period(8)
    device id 1: got motion event: t(-12, -125, 60) r(-105, 0, -122) period(8)
    ...
    device id 1: got button press event b(1)
    device id 1: got button release event b(1)
    ...

## Build

### Dependencies

* [libspacemouse](https://github.com/polyphemus/libspacemouse)

### Compile and install

    ./configure
    make
    sudo make install

### Uninstall

after at least `./configure` has run

    sudo make uninstall

## Examples

* cli examples:<br>
    some examples of how the different programs can be used, including selecting devices, led switching in a loop and filtering of events

* simple led deamon:<br>
    simple script for turning the LED on on device connect

* simple key map:<br>
    script for mapping events to keys with xdotool, for example: scrolling, zooming and killing applications

## Implementation

The `spacemouse-list`, `spacemouse-led` and `spacemouse-event` programs are actually symbolic links to the multi-call `spacemouse` binary.

The `spacemouse` program can also be called directly and use a command argument to specify which operations it should undertake: `list`, `led` and `event`. These commands take the same arguments as there symbolic link counter parts. When the `spacemouse` program is called without a command it defaults to the `list` command.

Use `-h/--help` for more information on the options and arguments the spacemouse program and it's symbolic links take.

