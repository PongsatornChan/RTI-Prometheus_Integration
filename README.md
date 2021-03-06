# rticonnextdds-prometheus

A Routing Service Processor plugin to publish DDS monitoring data as Prometheus metrics.

# Example Code: Routing Service Processor

Below there are the instructions to build and run this example. All the commands
and syntax used assume a Unix-based system. If you run this example in a
different architecture, please adapt the commands accordingly.

## Building the Example :wrench:
Let build libraries first.
1. yaml-cpp
    I. Navigate into the source directory third-party/yaml-cpp, and type:

    ```
    mkdir build
    cd build
    ```

    II. Run CMake. The basic syntax is:

    ```
    cmake .. -DBUILD_SHARED_LIBS=ON
    or debug version
    cmake .. -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Debug
    ```

    III. Build it!
    
    ```
    cmake --build .
    or debug version
    cmake --build . --config Debug
    ```

    **NOTE**: tp switch between release and debug, clean and rebuild is needed

    VI. To clean up, just remove the `build` directory.


2. prometheus-cpp
    For CMake builds don't forget to fetch the submodules first. Then build as usual.

    First, navigate into source directory, third-party/prometheus-cpp, then:

    ``` shell
    # fetch third-party dependencies
    git submodule init
    git submodule update

    mkdir _build
    cd _build

    # run cmake
    cmake .. -DBUILD_SHARED_LIBS=ON # or OFF for static libraries

    # build
    make -j 4

    # run tests
    ctest -V

    # install the libraries and headers
    mkdir -p deploy
    make DESTDIR=`pwd`/deploy install
    ```

To build this example, first run CMake to generate the corresponding build
files. We recommend you use a separate directory to store all the generated
files (e.g., ./build).

```sh
mkdir build
cd build
cmake -DBUILD_SHARED_LIBS=ON ..
```

Once you have run CMake, you will find a number of new files in your build
directory (the list of generated files will depend on the specific CMake
Generator). To build the example, run CMake as follows:

```sh
cmake --build .
```

**Note**: if you are using a multi-configuration generator, such as Visual
Studio solutions, you can specify the configuration mode to build as follows:

```sh
cmake --build . --config Release|Debug
```

Alternatively, you can use directly the generated infrastructure (e.g.,
Makefiles or Visual Studio Solutions) to build the example. If you generated
Makefiles in the configuration process, run make to build the example. Likewise,
if you generated a Visual Studio solution, open the solution and follow the
regular build process.

Upon success it will create a shared library file in the build directory.

## Running the Example

To run this example you will need two instances of *RTI Shapes Demo* and a
single instance of *RoutingService*.

To run *RoutingService*, you will need first to set up your environment as
follows:

```sh
export RTI_LD_LIBRARY_PATH=$NDDSHOME/lib/<ARCH>
```

where `<ARCH>` shall be replaced with the target architecture you used to build
the example in the previous step.

### MonitorProcessor

1.  Run one instance of *ShapesDemo* on domain 0. This will be the publisher
    application. Publish a blue square.

2.  Go to Configuration tab in under Controls, click Stop and choose the
    MonitorDemoLibrary::Monitor, then click Start.

    Note: You can check if Shapes Demo emits monitoring topics by
    ```sh
    bin/rtiddsspy -domainId 0 -printSample
    ```

3.  Now run *RoutingService* 

    Run the following command from the example build directory :

    ```sh
    $NDDSHOME/bin/rtiroutingservice -cfgFile ../RsMonitorProcessor.xml -cfgName MonitoringTopicExposer
    or Debug version
    $NDDSHOME/bin/rtiroutingserviced -cfgFile ../RsMonitorProcessor.xml -cfgName MonitoringTopicExposer
    ```
    **NOTE** To run Debug version, RsMonitorProcessor.xml content needed to be change a bit
    <plugin_library name="MonitorPluginLib">
        <processor_plugin name="MonitorProcessor">
            <dll>shapesprocessord</dll>
            <create_function>
                MonitorProcessorPlugin_create_processor_plugin
            </create_function>
        </processor_plugin>
    </plugin_library>

4.  Time to run *Prometheus*

    Run the following command from the Prometheus installation directory 
    For Mac, it is /Applications/prometheus-<ARCH>/. <ARCH> differs depend on your achitecture.

    ```sh
    ./prometheus --config.file=prometheus.yml --web.enable-lifecycle
    ```

5.  Now go to http://localhost:9090/graph in your browser.
    You can find call_on_data_available_total and data_writer_status, 
    which are metrics expose by our processor.
    NOTE: as of 6/25/20, You will see the value of both metrics 0. Work in progress

### Libraries used
I am learning CMake so if you faces problems involving libraries, CMake build, etc. 
I would recommend you check out and install these libraries and edit CMakeList.txt accordingly.
Thank you for your patient.
1. prometheus-cpp: https://github.com/jupp0r/prometheus-cpp
2. yaml-cpp: https://github.com/jbeder/yaml-cpp
3. boost: https://www.boost.org/doc/libs/1_73_0/more/getting_started/unix-variants.html

**NOTE** You need to have boost library installed to work. 
        For the first two library, You need to build prometheus-cpp and yaml-cpp. 
        You can find them in third-party directory. Both have detail build instruction in README files.

