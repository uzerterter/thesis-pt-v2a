#include "SoundRecommendationsComponent.h"

//==============================================================================
// Constructor
//==============================================================================
SoundRecommendationsComponent::SoundRecommendationsComponent()
{
    // Configure header label
    headerLabel.setText (juce::CharPointer_UTF8 ("\xf0\x9f\x8e\xb5 Database Recommendations [0/0]"), juce::dontSendNotification);  // 🎵
    headerLabel.setJustificationType (juce::Justification::centred);
    headerLabel.setFont (juce::Font (14.0f, juce::Font::bold));
    addAndMakeVisible (headerLabel);
    
    // Configure sound name label (display box)
    soundNameLabel.setText ("No results", juce::dontSendNotification);
    soundNameLabel.setJustificationType (juce::Justification::centredLeft);
    soundNameLabel.setFont (juce::Font (13.0f));
    soundNameLabel.setColour (juce::Label::backgroundColourId, juce::Colours::darkgrey.darker());
    soundNameLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    soundNameLabel.setColour (juce::Label::outlineColourId, juce::Colours::grey);
    addAndMakeVisible (soundNameLabel);
    
    // Configure buttons with click handlers
    prevButton.onClick = [this] { handlePrevClicked(); };
    addAndMakeVisible (prevButton);
    
    nextButton.onClick = [this] { handleNextClicked(); };
    addAndMakeVisible (nextButton);
    
    previewButton.onClick = [this] { handlePreviewClicked(); };
    addAndMakeVisible (previewButton);
    
    importButton.onClick = [this] { handleImportClicked(); };
    addAndMakeVisible (importButton);
    
    // Initially hidden until results are set
    setVisible (false);
}

//==============================================================================
// JUCE Component Lifecycle
//==============================================================================
void SoundRecommendationsComponent::paint (juce::Graphics& g)
{
    // Draw component background
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId).darker (0.3f));
    
    // Draw border
    g.setColour (juce::Colours::grey);
    g.drawRect (getLocalBounds(), 1);
}

void SoundRecommendationsComponent::resized()
{
    auto area = getLocalBounds().reduced (10);
    
    // Header: "🎵 Database Recommendations [1/5]"
    auto headerRow = area.removeFromTop (25);
    headerLabel.setBounds (headerRow);
    
    area.removeFromTop (8);  // Spacing
    
    // Sound name display box
    auto soundNameRow = area.removeFromTop (30);
    soundNameLabel.setBounds (soundNameRow);
    
    area.removeFromTop (12);  // Spacing
    
    // Button row: [◀ Prev] ... [▶ Preview] [↓ Import] ... [Next ▶]
    auto buttonRow = area.removeFromTop (28);
    
    const int buttonWidth = 100;
    const int spacing = 20;
    
    // Prev button at left edge w padding
    buttonRow.removeFromLeft (20);
    prevButton.setBounds (buttonRow.removeFromLeft (buttonWidth));

    // Next button at right edge w padding
    buttonRow.removeFromRight (20);
    nextButton.setBounds (buttonRow.removeFromRight (buttonWidth));
    
    // Center Preview and Import in remaining space
    const int centerWidth = buttonWidth * 2 + spacing;
    const int centerStartX = (buttonRow.getWidth() - centerWidth) / 2;
    
    buttonRow.removeFromLeft (centerStartX);
    previewButton.setBounds (buttonRow.removeFromLeft (buttonWidth));
    buttonRow.removeFromLeft (spacing);
    importButton.setBounds (buttonRow.removeFromLeft (buttonWidth));
}

//==============================================================================
// Public Interface
//==============================================================================
void SoundRecommendationsComponent::setResults (const std::vector<SoundResult>& results)
{
    soundResults = results;
    currentIndex = 0;
    
    updateDisplay();
    setVisible (!soundResults.empty());
}

void SoundRecommendationsComponent::clearResults()
{
    soundResults.clear();
    currentIndex = 0;
    updateDisplay();
    setVisible (false);
}

const SoundResult* SoundRecommendationsComponent::getCurrentSound() const
{
    if (soundResults.empty() || currentIndex < 0 || currentIndex >= static_cast<int>(soundResults.size()))
        return nullptr;
    
    return &soundResults[currentIndex];
}

//==============================================================================
// Event Handlers
//==============================================================================
void SoundRecommendationsComponent::handlePrevClicked()
{
    if (soundResults.empty())
        return;
    
    currentIndex--;
    if (currentIndex < 0)
        currentIndex = static_cast<int>(soundResults.size()) - 1;  // Wrap to last
    
    updateDisplay();
}

void SoundRecommendationsComponent::handleNextClicked()
{
    if (soundResults.empty())
        return;
    
    currentIndex++;
    if (currentIndex >= static_cast<int>(soundResults.size()))
        currentIndex = 0;  // Wrap to first
    
    updateDisplay();
}

void SoundRecommendationsComponent::handlePreviewClicked()
{
    juce::Logger::writeToLog ("[SoundRec] Preview button clicked");
    
    auto* sound = getCurrentSound();
    if (!sound)
    {
        juce::Logger::writeToLog ("[SoundRec] ERROR: No current sound");
        return;
    }
    
    juce::Logger::writeToLog ("[SoundRec] Current sound: ID=" + juce::String (sound->id) + ", " + sound->description);
    
    if (!onPreview)
    {
        juce::Logger::writeToLog ("[SoundRec] ERROR: onPreview callback is not set!");
        return;
    }
    
    juce::Logger::writeToLog ("[SoundRec] Calling onPreview callback...");
    onPreview (*sound);
}

void SoundRecommendationsComponent::handleImportClicked()
{
    juce::Logger::writeToLog ("[SoundRec] Import button clicked");
    
    auto* sound = getCurrentSound();
    if (!sound)
    {
        juce::Logger::writeToLog ("[SoundRec] ERROR: No current sound");
        return;
    }
    
    juce::Logger::writeToLog ("[SoundRec] Current sound: ID=" + juce::String (sound->id) + ", " + sound->description);
    
    if (!onImport)
    {
        juce::Logger::writeToLog ("[SoundRec] ERROR: onImport callback is not set!");
        return;
    }
    
    juce::Logger::writeToLog ("[SoundRec] Calling onImport callback...");
    onImport (*sound);
}

void SoundRecommendationsComponent::updateDisplay()
{
    if (soundResults.empty())
    {
        headerLabel.setText (juce::CharPointer_UTF8 ("\xf0\x9f\x8e\xb5 Database Recommendations [0/0]"), juce::dontSendNotification);
        soundNameLabel.setText ("No results", juce::dontSendNotification);
        
        prevButton.setEnabled (false);
        nextButton.setEnabled (false);
        previewButton.setEnabled (false);
        importButton.setEnabled (false);
    }
    else
    {
        // Update header with current position
        juce::String headerText = juce::CharPointer_UTF8 ("\xf0\x9f\x8e\xb5 Database Recommendations [");
        headerText << (currentIndex + 1) << "/" << soundResults.size() << "]";
        headerLabel.setText (headerText, juce::dontSendNotification);
        
        // Update sound name
        auto& currentSound = soundResults[currentIndex];
        juce::String displayText = juce::CharPointer_UTF8 ("\xf0\x9f\x94\x8a ");  // 🔊
        displayText << currentSound.description;
        soundNameLabel.setText (displayText, juce::dontSendNotification);
        
        // Enable all buttons
        prevButton.setEnabled (true);
        nextButton.setEnabled (true);
        previewButton.setEnabled (true);
        importButton.setEnabled (true);
    }
}
