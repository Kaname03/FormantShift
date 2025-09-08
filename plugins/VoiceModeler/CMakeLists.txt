juce_add_plugin(VoiceModeler
    COMPANY_NAME "YourCompany"
    BUNDLE_ID com.yourcompany.voicemodeler
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE YCmp
    PLUGIN_CODE VMod
    FORMATS VST3
    PRODUCT_NAME "VoiceModeler"
    VERSION "0.1.0"
    VST3_CAN_REPLACE_VST2 FALSE      # ★ ここ！
)

target_compile_definitions(VoiceModeler PRIVATE
    JucePlugin_VST3CanReplaceVST2=0     # JUCE 内部で参照される定義
    JUCE_VST3_CAN_REPLACE_VST2=0        # 版違い対策（新旧どちらでも抑止）
)


target_sources(VoiceModeler
    PRIVATE
      Source/PluginProcessor.cpp
      Source/PluginProcessor.h
      Source/PluginEditor.cpp
      Source/PluginEditor.h
)

target_compile_features(VoiceModeler PRIVATE cxx_std_17)

target_link_libraries(VoiceModeler
    PRIVATE
      juce::juce_audio_utils
      juce::juce_audio_processors
      juce::juce_dsp
      juce::juce_gui_extra
      juce::juce_core
)

if(MSVC)
  target_compile_options(VoiceModeler PRIVATE /permissive- /EHsc /Zc:preprocessor)
else()
  target_compile_options(VoiceModeler PRIVATE -Wall -Wextra -Wpedantic)
endif()
