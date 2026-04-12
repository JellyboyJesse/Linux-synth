cmake_minimum_required(VERSION 3.22)
project(LinuxSynth VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ---- Fetch JUCE 7 -------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.9
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(JUCE)

# ---- Application target -------------------------------------------------
juce_add_gui_app(LinuxSynth
    PRODUCT_NAME  "Linux Synth"
    COMPANY_NAME  "LinuxSynth"
    BUNDLE_ID     "com.linuxsynth.app"
)

juce_generate_juce_header(LinuxSynth)

target_sources(LinuxSynth
    PRIVATE
        Source/Main.cpp
        Source/MainComponent.cpp
        Source/AudioEngine.cpp
        Source/MatrixView.cpp
        Source/EffectsView.cpp
        Source/WaveformDisplay.cpp
        Source/TransportBar.cpp
        Source/WireframeLookAndFeel.cpp
)

target_compile_definitions(LinuxSynth
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_ALSA=1
        JUCE_JACK=1
        JUCE_ASIO=0
        JUCE_WASAPI=0
        JUCE_DIRECTSOUND=0
        JUCE_COREAUDIO=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:LinuxSynth,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:LinuxSynth,JUCE_VERSION>"
)

target_link_libraries(LinuxSynth
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
