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
    
    // First button row: [◀ Prev]   [▶ Preview]   [Next ▶]
    auto firstButtonRow = area.removeFromTop (28);
    
    const int buttonWidth = 100;
    const int spacing = 20;
    
    // Calculate total width and center the buttons
    const int totalWidth = buttonWidth * 3 + spacing * 2;
    const int startX = (firstButtonRow.getWidth() - totalWidth) / 2;
    
    auto prevArea = firstButtonRow.removeFromLeft (startX + buttonWidth);
    prevArea.removeFromRight (buttonWidth);
    prevButton.setBounds (prevArea);
    
    firstButtonRow.removeFromLeft (spacing);
    previewButton.setBounds (firstButtonRow.removeFromLeft (buttonWidth));
    
    firstButtonRow.removeFromLeft (spacing);
    nextButton.setBounds (firstButtonRow.removeFromLeft (buttonWidth));
    
    area.removeFromTop (8);  // Spacing
    
    // Second button row: [↓ Import] (centered)
    auto secondButtonRow = area.removeFromTop (28);
    const int importWidth = 120;
    const int importX = (secondButtonRow.getWidth() - importWidth) / 2;
    importButton.setBounds (secondButtonRow.removeFromLeft (importX + importWidth).removeFromRight (importWidth));
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
    auto* sound = getCurrentSound();
    if (sound && onPreview)
        onPreview (*sound);
}

void SoundRecommendationsComponent::handleImportClicked()
{
    auto* sound = getCurrentSound();
    if (sound && onImport)
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
