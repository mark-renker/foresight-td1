# ---- JUCE: prefer a local checkout, else fetch -------------------------------
set(TD1_JUCE_DIR "" CACHE PATH "Path to a JUCE 8 checkout (optional)")
if(NOT TD1_JUCE_DIR)
  foreach(cand "$ENV{HOME}/dev/JUCE" "$ENV{HOME}/JUCE")
    if(EXISTS "${cand}/CMakeLists.txt")
      set(TD1_JUCE_DIR "${cand}")
      break()
    endif()
  endforeach()
endif()
if(TD1_JUCE_DIR)
  message(STATUS "TD-1: using JUCE at ${TD1_JUCE_DIR}")
  add_subdirectory("${TD1_JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")
else()
  include(FetchContent)
  FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
                            GIT_TAG 8.0.14 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()

set(TD1_FORMATS VST3 Standalone)
if(APPLE)
  list(APPEND TD1_FORMATS AU)
endif()

juce_add_plugin(ForesightTD1
  PRODUCT_NAME "Foresight TD-1"
  COMPANY_NAME "Black Tie Ventures"
  BUNDLE_ID com.blacktieventures.foresighttd1
  PLUGIN_MANUFACTURER_CODE Bktv
  PLUGIN_CODE Ftd1
  FORMATS ${TD1_FORMATS}
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  VST3_CATEGORIES Fx Dynamics
  AU_MAIN_TYPE kAudioUnitType_Effect
  COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(TD1Assets SOURCES
  assets/fonts/IBMPlexMono-Regular.ttf
  assets/fonts/IBMPlexMono-Medium.ttf
  assets/fonts/BigShouldersDisplay-Bold.ttf)

target_sources(ForesightTD1 PRIVATE
  src/Parameters.cpp
  src/Presets.cpp
  src/PluginProcessor.cpp
  src/PluginEditor.cpp
  src/ui/LookAndFeel.cpp
  src/ui/Components.cpp)
target_include_directories(ForesightTD1 PRIVATE src)

target_compile_definitions(ForesightTD1 PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(ForesightTD1
  PRIVATE
    td1_core
    TD1Assets
    juce::juce_audio_utils
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# ---- CLAP (opt-in: -DTD1_ENABLE_CLAP=ON fetches clap-juce-extensions) ---------
if(TD1_ENABLE_CLAP)
  include(FetchContent)
  FetchContent_Declare(clap_juce_extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG main
    GIT_SHALLOW TRUE
    GIT_SUBMODULES_RECURSE TRUE)
  FetchContent_MakeAvailable(clap_juce_extensions)
  clap_juce_extensions_plugin(TARGET ForesightTD1
    CLAP_ID "com.blacktieventures.foresighttd1"
    CLAP_FEATURES audio-effect transient-shaper dynamics stereo mono)
endif()

# ---- Snapshot tool: renders the editor to PNG (UI review / docs) -------------
option(TD1_BUILD_SNAPSHOT "Build the td1_snapshot UI render tool" ON)
if(TD1_BUILD_SNAPSHOT)
  juce_add_console_app(td1_snapshot PRODUCT_NAME "td1_snapshot")
  target_sources(td1_snapshot PRIVATE
    ${PROJECT_SOURCE_DIR}/tools/Snapshot.cpp
    src/Parameters.cpp src/Presets.cpp src/PluginProcessor.cpp src/PluginEditor.cpp
    src/ui/LookAndFeel.cpp src/ui/Components.cpp)
  target_include_directories(td1_snapshot PRIVATE src)
  target_compile_definitions(td1_snapshot PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Foresight TD-1"
    JUCE_STANDALONE_APPLICATION=1)
  target_link_libraries(td1_snapshot PRIVATE td1_core TD1Assets
    juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
