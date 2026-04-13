#pragma once

//==============================================================================
// ScaleTable
//
// Static lookup table of musical scale interval sets.
// Each entry stores the semitone offsets from the root note (0-based).
// Use isInScale() to test whether a given absolute semitone is in a scale.
//==============================================================================
namespace ScaleTable
{

struct Scale
{
    const char* name;
    int         numNotes;
    int         intervals[12]; // semitone offsets from root (unused slots = 0)
};

static constexpr Scale kScales[] =
{
    { "Major",         7,  { 0, 2, 4, 5, 7, 9, 11 } },
    { "Natural Minor", 7,  { 0, 2, 3, 5, 7, 8, 10 } },
    { "Dorian",        7,  { 0, 2, 3, 5, 7, 9, 10 } },
    { "Phrygian",      7,  { 0, 1, 3, 5, 7, 8, 10 } },
    { "Lydian",        7,  { 0, 2, 4, 6, 7, 9, 11 } },
    { "Mixolydian",    7,  { 0, 2, 4, 5, 7, 9, 10 } },
    { "Locrian",       7,  { 0, 1, 3, 5, 6, 8, 10 } },
    { "Penta. Maj",    5,  { 0, 2, 4, 7, 9 } },
    { "Penta. Min",    5,  { 0, 3, 5, 7, 10 } },
    { "Blues",         6,  { 0, 3, 5, 6, 7, 10 } },
    { "Whole Tone",    6,  { 0, 2, 4, 6, 8, 10 } },
    { "Chromatic",     12, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },
};

static constexpr int kNumScales = int(sizeof(kScales) / sizeof(kScales[0]));

static const char* const kNoteNames[12] =
    { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

/// Returns true if @p semitone (0-11) is a member of @p scaleIndex with
/// the given @p rootNote (0-11, C=0).
inline bool isInScale(int semitone, int scaleIndex, int rootNote) noexcept
{
    const int si = scaleIndex < 0 || scaleIndex >= kNumScales ? 0 : scaleIndex;
    const Scale& s = kScales[si];
    const int relative = ((semitone - rootNote) % 12 + 12) % 12;
    for (int i = 0; i < s.numNotes; ++i)
        if (s.intervals[i] == relative)
            return true;
    return false;
}

} // namespace ScaleTable
