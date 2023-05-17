//
//  main.c
//  Extension
//
//  Created by Ezra Weller, 5/12/2023
//

#include <stdio.h>
#include <stdlib.h>

#include <pd_api.h>

// TYPES
enum NodeType { Dead, Strong, Weak };
struct Volume { float l; float r; };
struct PitchSet
{
    int common[4];
    int commonCount;
    int rare[4];
    int rareCount;
};
struct PitchField
{
    int pitches[12];
};

// DECLARED METHODS
static int update(void* userdata);

// GLOBAL STATE
// #define FONT_PATH "/System/Fonts/Asheville-Sans-14-Bold.pft"

#define SAMPLE_RATE 44100

#define MAX_NODES 12

#define NODE_MIN_ATTACK_1 0.01f
#define NODE_MAX_ATTACK_1 0.2f
#define NODE_MIN_DECAY_1 0.05f
#define NODE_MAX_DECAY_1 0.3f
#define NODE_MIN_SUSTAIN_1 0.06f
#define NODE_MAX_SUSTAIN_1 0.3f
#define NODE_MIN_RELEASE_1 0.2f
#define NODE_MAX_RELEASE_1 1.0f

#define NODE_MIN_ATTACK_2 0.2f
#define NODE_MAX_ATTACK_2 1.0f
#define NODE_MIN_DECAY_2 0.5f
#define NODE_MAX_DECAY_2 1.0f
#define NODE_MIN_SUSTAIN_2 0.1f
#define NODE_MAX_SUSTAIN_2 0.5f
#define NODE_MIN_RELEASE_2 0.5f
#define NODE_MAX_RELEASE_2 4.0f

#define MIN_FREQ_MOD_RATE 0.001f
#define MAX_FREQ_MOD_RATE 0.3f
#define MIN_FREQ_MOD_DEPTH 0.0f
#define MAX_FREQ_MOD_DEPTH 0.04f
#define MIN_FREQ_MOD_PHASE 0.0f
#define MAX_FREQ_MOD_PHASE 1.0f

#define MIN_AMP_MOD_RATE 0.1f
#define MAX_AMP_MOD_RATE 50.0f
#define MIN_AMP_MOD_DEPTH 0.0f
#define MAX_AMP_MOD_DEPTH 0.5f
#define MIN_AMP_MOD_PHASE 0.0f
#define MAX_AMP_MOD_PHASE 1.0f

#define NODE_MIN_LEN 0.02f
#define NODE_MAX_LEN 4.0f

#define NODE_MIN_VOLUME 0.1f
#define NODE_MAX_VOLUME 1.0f

#define NODE_MIN_PULSE_MOD 0.03f
#define NODE_MAX_PULSE_MOD 5.0f

#define MIN_TIME_VELOCITY 0.5f
#define MAX_TIME_VELOCITY 2.5f

// of 10
#define RARE_PITCH_ODDS 2

#define MIDI_START 21
#define NODE_MIN_OCTAVE 0
#define NODE_MAX_OCTAVE 6

#define MIN_LOWS 2.0f
#define MAX_LOWS 8.0f
#define MIN_HIGHS 15.0f
#define MAX_HIGHS 0.5f

#define GRID_SCALE 0.01f

// misc
const PlaydateAPI* PD;
LCDFont* FONT = NULL;
const char* ERR;
const int SLOW_BASE_PULSE = SAMPLE_RATE * 10;
const int HIGH_BASE_PULSE = SAMPLE_RATE / 15;
const int NODE_LIFETIME = SAMPLE_RATE * 16;
const int NODE_FADE_BUFFER = SAMPLE_RATE * 4;
const float CENTER_X = (float)LCD_COLUMNS / 2.0f;
const float CENTER_Y = (float)LCD_ROWS / 2.0f;
static uint32_t CURRENT_TIME;
static uint32_t LAST_REAL_TIME;
float TIME_VELOCITY;
static uint32_t NEXT_TOUCH;
const int TOUCH_RATE = SAMPLE_RATE / 10;
static float MAX_DISTANCE_FROM_CENTER;
static float INVERSE_MAX_DIST_FROM_CENTER;
static float INVERSE_FADE_BUFFER;
const struct PitchField PITCH_FIELD_SET[4] =
{
    {{ 0, 0, 2, 5, 7, 7, 12, 12, 14, 17, 19, 23 }},
    {{ 2, 4, 4, 5, 9, 14, 14, 16, 19, 21, 22, 24 }},
    {{ 3, 3+2, 3+4, 3+5, 3+7, 3+9, 3+11, 3+12, 3+14, 3+16, 3+17, 3+23 }},
    {{ -2, -2, -2+4, -2+7, -2+9, -2+10, -2+12, -2+14, -2+16, -2+19, -2+20, -2+22 }}
};
static int PITCH_FIELD_ID;
static struct PitchField PITCH_FIELD;
const struct PitchSet PITCH_SETS[4] =
{
    { {0, 2, 3}, 3, {1}, 1 },
    { {3, 5}, 2, {4}, 1 },
    { {5, 6, 7}, 3, {8}, 1 },
    { {7, 10}, 2, {9, 11}, 2 }
};
static uint32_t NEXT_PITCH_FIELD_CHANGE;
const int CHANGE_PITCH_FIELD_RATE = SAMPLE_RATE * 30;
const int GRID_HEIGHT = (float)LCD_ROWS * GRID_SCALE;
const int GRID_WIDTH = (float)LCD_COLUMNS * GRID_SCALE;
static int GRID_POINT_NODE_COUNT[(int)((float)LCD_ROWS * GRID_SCALE)][(int)((float)LCD_COLUMNS * GRID_SCALE)];

// global sound
float BASE_PULSE;
SoundChannel *CHANNEL;
TwoPoleFilter *HIGH_SHELF;
TwoPoleFilter *LOW_SHELF;
/*
DelayLine *DELAY;
DelayLineTap *DELAY_OUT;
 */

// player state
float PLAYER_X;
float PLAYER_Y;
LCDSprite *PLAYER_SPRITE;

// node state
int32_t NODE_NEED_TOUCH[MAX_NODES];
int NODE_X[MAX_NODES];
int NODE_Y[MAX_NODES];
enum NodeType NODE_TYPE[MAX_NODES];
int NODE_DEATH_TIME[MAX_NODES];
int NODE_GRID_X[MAX_NODES];
int NODE_GRID_Y[MAX_NODES];
LCDSprite *NODE_1_SPRITE_MASTER;
LCDSprite *NODE_2_SPRITE_MASTER;
LCDSprite *NODE_SPRITE[MAX_NODES];
int NODE_ANIM_STATE[MAX_NODES];

// node management
int LAST_FREED_NODE;
int LIVE_NODE_COUNT = 0;

// node sound
struct PDSynth *NODE_SYNTH[MAX_NODES];
uint32_t NODE_NEXT_PULSE[MAX_NODES];
float NODE_PULSE_MOD[MAX_NODES];
struct Volume NODE_FADE_VOL[MAX_NODES];
float NODE_LEN[MAX_NODES];
int NODE_PITCH_SET[MAX_NODES];
// 0 to 6
int NODE_OCTAVE[MAX_NODES];

// images
LCDBitmap *PLAYER_BM;
LCDBitmap *ROTATED_PLAYER_BM;
LCDBitmap *NODE_BMT_1[8];
LCDBitmap *NODE_BMT_2[8];

// math
float __attribute__((always_inline)) lerp(float w, float h, float alpha)
{
    return w * (1 - alpha) + h * alpha;
}

float __attribute__((always_inline)) distance(int x1, int y1, int x2, int y2)
{
    float side1 = fabsf((float)(x1 - x2));
    float side2 = fabsf((float)(y1 - y2));
    return sqrtf(side1 * side1 + side2 * side2);
}

// specialized utility methods for this toy
static float closenessToCenter(int x, int y)
{
    return (MAX_DISTANCE_FROM_CENTER - distance(x, y, CENTER_X, CENTER_Y)) / MAX_DISTANCE_FROM_CENTER;
}

static void ageEarliestNode()
{
    int i;
    for (i = 0; i < MAX_NODES; i++)
    {
        if (NODE_TYPE[i] != Dead)
        {
            NODE_DEATH_TIME[i] = CURRENT_TIME + NODE_FADE_BUFFER;
            return;
        }
    }
}

static int quadrantOfPoint(int X, int Y)
{
    if (X >= CENTER_X)
    {
        if (Y >= CENTER_Y) return 0;
        return 1;
    }
    if (Y >= CENTER_Y) return 2;
    return 3;
}

static int gridScaleInt(int n) { return floorf((float)n * GRID_SCALE); }

static float closenessToOtherNodes(int nodeID)
{
    int rawScore = 0;
    for (int x = -1; x < 2; x++)
    {
        for (int y = -1; y < 2; y++)
        {
            int gridX = x + NODE_GRID_X[nodeID];
            int gridY = y + NODE_GRID_Y[nodeID];
            if (gridX > -1 && gridY > -1 && gridX < GRID_WIDTH && gridY < GRID_HEIGHT)
            {
                rawScore += GRID_POINT_NODE_COUNT[gridY][gridX];
            }
        }
    }
    // don't count self
    rawScore--;
    if (rawScore > MAX_NODES) rawScore = MAX_NODES;
    return (float)rawScore / (float)MAX_NODES;
}

// core methods for this toy
static void freeNode(int nodeID)
{
    // reset state
    NODE_TYPE[nodeID] = Dead;
    NODE_FADE_VOL[nodeID].l = -1.0f;
    GRID_POINT_NODE_COUNT[NODE_GRID_Y[nodeID]][NODE_GRID_X[nodeID]] -= 1;
    
    // free memory
    const struct playdate_sound *pdSound = PD->sound;
    pdSound->channel->removeSource(CHANNEL, (SoundSource *)NODE_SYNTH[nodeID]);
    pdSound->lfo->freeLFO(
                          (PDSynthLFO *)pdSound->synth->getFrequencyModulator(NODE_SYNTH[nodeID])
                          );
    pdSound->lfo->freeLFO(
                          (PDSynthLFO *)pdSound->synth->getAmplitudeModulator(NODE_SYNTH[nodeID])
                          );
    pdSound->synth->freeSynth(NODE_SYNTH[nodeID]);
    PD->sprite->freeSprite(NODE_SPRITE[nodeID]);
    
    // node management
    LAST_FREED_NODE = nodeID;
    LIVE_NODE_COUNT--;
}

static void pulseNode(int nodeID)
{
    if (CURRENT_TIME >= NODE_NEXT_PULSE[nodeID])
    {
        int fieldPitch;
        int pitch;
        struct PitchSet set = PITCH_SETS[NODE_PITCH_SET[nodeID]];
        if (rand() % 10 > RARE_PITCH_ODDS)
        {
            fieldPitch = PITCH_FIELD.pitches[set.common[rand() % set.commonCount]];
        } else {
            fieldPitch = PITCH_FIELD.pitches[set.rare[rand() % set.rareCount]];
        }
        pitch = MIDI_START + fieldPitch + 12 * NODE_OCTAVE[nodeID];
        PD->sound->synth->playMIDINote(
                                       NODE_SYNTH[nodeID],
                                       pitch,
                                       lerp(0.5f, 1.0f, (float)(rand() % 100) * 0.03),
                                       NODE_LEN[nodeID],
                                       0
                                       );
        float salt = (float)(rand() % 1000);
        NODE_NEXT_PULSE[nodeID] = CURRENT_TIME + NODE_PULSE_MOD[nodeID] * lerp(SLOW_BASE_PULSE, HIGH_BASE_PULSE, BASE_PULSE) + salt;
    }
}

// update node sound and management & maybe pulse the node
static void touchNode(int nodeID)
{
    // if node should die, kill it and return early
    if (CURRENT_TIME > NODE_DEATH_TIME[nodeID])
    {
        freeNode(nodeID);
        return;
    }
    
    // do look ups
    const struct playdate_sound_synth *pdSynth = PD->sound->synth;
    PDSynth *synth = NODE_SYNTH[nodeID];
    
    float x, y;
    PD->sprite->getPosition(NODE_SPRITE[nodeID], &x, &y);
    float nodeCloseness = closenessToOtherNodes(nodeID);
    
    // touch timbre
    {
        const struct playdate_sound_lfo *pdLFO = PD->sound->lfo;
        
        // freq mod
        struct PDSynthLFO* freqMod =
            (PDSynthLFO *)pdSynth->getFrequencyModulator(NODE_SYNTH[nodeID]);
        // closeness to other nodes = more intense
        float freqModAlpha = 1.0f - nodeCloseness;
        pdLFO->setRate(freqMod, lerp(MIN_FREQ_MOD_RATE, MAX_FREQ_MOD_RATE, freqModAlpha));
        pdLFO->setPhase(freqMod, lerp(MIN_FREQ_MOD_PHASE, MAX_FREQ_MOD_PHASE, freqModAlpha));
        pdLFO->setDepth(freqMod, lerp(MIN_FREQ_MOD_DEPTH, MAX_FREQ_MOD_DEPTH, freqModAlpha));
        
        // amp mod
        struct PDSynthLFO* ampMod =
            (PDSynthLFO *)pdSynth->getAmplitudeModulator(NODE_SYNTH[nodeID]);
        // closeness to other nodes = more intense
        float ampModAlpha = 1.0f - nodeCloseness;
        pdLFO->setRate(ampMod, lerp(MIN_AMP_MOD_RATE, MAX_AMP_MOD_RATE, ampModAlpha));
        pdLFO->setPhase(ampMod, lerp(MIN_AMP_MOD_PHASE, MAX_AMP_MOD_PHASE, ampModAlpha));
        pdLFO->setDepth(ampMod, lerp(MIN_AMP_MOD_DEPTH, MAX_AMP_MOD_DEPTH, ampModAlpha));
        
        pdSynth->setFrequencyModulator(synth, (PDSynthSignalValue *)freqMod);
        pdSynth->setAmplitudeModulator(synth, (PDSynthSignalValue *)ampMod);
    }
    
    // touch octave
    NODE_OCTAVE[nodeID] = floorf(lerp(NODE_MIN_OCTAVE, NODE_MAX_OCTAVE, 1.0f - (float)y / (float)LCD_ROWS));
    
    // touch envelope
    {
        // node type 1 = harsher
        // less life left = smoother
        float envAlpha = (float)(CURRENT_TIME - (NODE_DEATH_TIME[nodeID] - NODE_LIFETIME)) / (float)(NODE_LIFETIME);
        if (NODE_TYPE[nodeID] == 1)
        {
            pdSynth->setAttackTime(synth, lerp(NODE_MIN_ATTACK_1, NODE_MAX_ATTACK_1, envAlpha));
            pdSynth->setDecayTime(synth, lerp(NODE_MIN_DECAY_1, NODE_MAX_DECAY_1, envAlpha));
            pdSynth->setSustainLevel(synth, lerp(NODE_MIN_SUSTAIN_1, NODE_MAX_SUSTAIN_1, envAlpha));
            pdSynth->setReleaseTime(synth, lerp(NODE_MIN_RELEASE_1, NODE_MAX_RELEASE_1, envAlpha));
        }
        else
        {
            pdSynth->setAttackTime(synth, lerp(NODE_MIN_ATTACK_2, NODE_MAX_ATTACK_2, envAlpha));
            pdSynth->setDecayTime(synth, lerp(NODE_MIN_DECAY_2, NODE_MAX_DECAY_2, envAlpha));
            pdSynth->setSustainLevel(synth, lerp(NODE_MIN_SUSTAIN_2, NODE_MAX_SUSTAIN_2, envAlpha));
            pdSynth->setReleaseTime(synth, lerp(NODE_MIN_RELEASE_2, NODE_MAX_RELEASE_2, envAlpha));
        }
    }
    
    
    // touch volume
    // closeness to other nodes = louder
    // slight hairpin in and fadeout with lifetime (can replace the whole fadeout mechanic probably)
    {
        float lifespanAlpha = 1.0f;
        if (CURRENT_TIME > NODE_DEATH_TIME[nodeID] - NODE_FADE_BUFFER)
        {
            lifespanAlpha = (float)(NODE_DEATH_TIME[nodeID] - CURRENT_TIME) * INVERSE_FADE_BUFFER;
        }
        float baseVol = lifespanAlpha * (0.8f * nodeCloseness + 0.2f);
        float leftPan = (float)x / (float)LCD_COLUMNS;
        pdSynth->setVolume(
                           synth,
                           lerp(0.0f, baseVol, leftPan),
                           lerp(0.0f, baseVol, 1.0f - leftPan)
                           );
    }
    
    // touch length
    // closeness to center = shorter
    float centerCloseness = closenessToCenter(x, y);
    NODE_LEN[nodeID] = lerp(NODE_MIN_LEN, NODE_MAX_LEN, 1.0f - centerCloseness);
    
    // touch pulse
    // closeness to center = faster
    NODE_PULSE_MOD[nodeID] = BASE_PULSE * lerp(NODE_MAX_PULSE_MOD, NODE_MIN_PULSE_MOD, centerCloseness);
    
    // touch position
    x += (float)(rand() % 2) - 0.5f;
    y += (float)(rand() % 2) - 0.5f;
    if (x > (float)LCD_COLUMNS) x -= (float)LCD_COLUMNS;
    if (y > (float)LCD_ROWS) y -= (float)LCD_ROWS;
    if (x < 0.0f) x += (float)LCD_COLUMNS;
    if (y < 0.0f) y += (float)LCD_ROWS;
    PD->sprite->moveTo(NODE_SPRITE[nodeID], x, y);
    
    // touch sprites
    NODE_ANIM_STATE[nodeID] = (NODE_ANIM_STATE[nodeID] + 1) % 8;
    if (NODE_TYPE[nodeID] == 1)
    {
        PD->sprite->setImage(NODE_SPRITE[nodeID], NODE_BMT_1[NODE_ANIM_STATE[nodeID]], kBitmapUnflipped);
    }
    else if (NODE_TYPE[nodeID] == 2)
    {
        PD->sprite->setImage(NODE_SPRITE[nodeID], NODE_BMT_2[NODE_ANIM_STATE[nodeID]], kBitmapUnflipped);
    }
    
    // pulse
    pulseNode(nodeID);
}

static void makeNode(enum NodeType type, int X, int Y)
{
    // if node count is max - 1 (i.e. room for only 1 more node), trigger an older node to fade out and die
    // (this means the real max is actually max - 1)
    if (!(LIVE_NODE_COUNT < MAX_NODES - 1)) ageEarliestNode();
    
    // if we have too many nodes, return early
    if (LIVE_NODE_COUNT > MAX_NODES - 1) return;
    
    int nodeID = -1;
    if (LAST_FREED_NODE == -1)
    {
        while (nodeID == -1)
        {
            for (int i = 0; i < MAX_NODES; i++)
            {
                if (NODE_TYPE[i] == Dead) nodeID = i;
            }
        }
    }
    else
    {
        nodeID = LAST_FREED_NODE;
        LAST_FREED_NODE = -1;
    }
    
    // node state
    NODE_TYPE[nodeID] = type;
    NODE_DEATH_TIME[nodeID] = CURRENT_TIME + NODE_LIFETIME;
    NODE_NEED_TOUCH[nodeID] = 1;
    int gridX = gridScaleInt(X);
    int gridY = gridScaleInt(Y);
    NODE_GRID_X[nodeID] = gridX;
    NODE_GRID_Y[nodeID] = gridY;
    GRID_POINT_NODE_COUNT[gridY][gridX] += 1;
    
    // node sound
    const struct playdate_sound *pdSound = PD->sound;
    PDSynth *synth = pdSound->synth->newSynth();
    NODE_SYNTH[nodeID] = synth;
    PDSynthLFO *freqMod;
    PDSynthLFO *ampMod;
    if (type == Strong)
    {
        pdSound->synth->setWaveform(synth, kWaveformSawtooth);
        ampMod = pdSound->lfo->newLFO(kLFOTypeTriangle);
        freqMod = pdSound->lfo->newLFO(kLFOTypeTriangle);
    } else
    {
        pdSound->synth->setWaveform(synth, kWaveformSine);
        ampMod = pdSound->lfo->newLFO(kLFOTypeSine);
        freqMod = pdSound->lfo->newLFO(kLFOTypeSine);
    }
    pdSound->lfo->setCenter(freqMod, 0.5f);
    pdSound->lfo->setCenter(ampMod, 0.5f);
    pdSound->synth->setFrequencyModulator(synth, (PDSynthSignalValue *)freqMod);
    pdSound->synth->setAmplitudeModulator(synth, (PDSynthSignalValue *)ampMod);
    NODE_NEXT_PULSE[nodeID] = CURRENT_TIME;
    NODE_PULSE_MOD[nodeID] = lerp(NODE_MIN_PULSE_MOD, NODE_MAX_PULSE_MOD, 0.5f);
    NODE_PITCH_SET[nodeID] = quadrantOfPoint(X, Y);
    pdSound->channel->addSource(CHANNEL, (SoundSource *)synth);
    
    // node sprite
    const struct playdate_sprite *sprite = PD->sprite;
    if (type == 1) { NODE_SPRITE[nodeID] = sprite->copy(NODE_1_SPRITE_MASTER); }
    else if (type == 2) { NODE_SPRITE[nodeID] = sprite->copy(NODE_2_SPRITE_MASTER); }
    sprite->moveTo(NODE_SPRITE[nodeID], X, Y);
    sprite->addSprite(NODE_SPRITE[nodeID]);
    NODE_ANIM_STATE[nodeID] = 0;
    
    // always touch right away
    touchNode(nodeID);
    
    // node management
    LIVE_NODE_COUNT++;
}

static void setup(PlaydateAPI* pd)
{
    PD = (PlaydateAPI *)pd;
    CURRENT_TIME = PD->sound->getCurrentTime();
    NEXT_TOUCH = CURRENT_TIME + TOUCH_RATE;
    
    BASE_PULSE = 0.5f;
    TIME_VELOCITY = 1.0f;
    MAX_DISTANCE_FROM_CENTER = distance(0, 0, CENTER_X, CENTER_Y);
    INVERSE_MAX_DIST_FROM_CENTER = 1.0f / MAX_DISTANCE_FROM_CENTER;
    INVERSE_FADE_BUFFER = 1.0f / (float)NODE_FADE_BUFFER;
    LAST_FREED_NODE = -1;
    
    int i;
    for (i = 0; i < MAX_NODES; i++)
    {
        // Default all nodes to dead
        NODE_TYPE[i] = Dead;
        
        // Default all fade vol to unset
        NODE_FADE_VOL[i].l = -1.0f;
    }
    
    for (i = 0; i < GRID_HEIGHT; i++)
    {
        for (int j = 0; j < GRID_WIDTH; j++)
        {
            GRID_POINT_NODE_COUNT[i][j] = 0;
        }
        
    }
    
    // load images
    PLAYER_BM = PD->graphics->loadBitmap("images/player", &ERR);
    NODE_BMT_1[0] = PD->graphics->loadBitmap("images/Node_1/type_1_1", &ERR);
    NODE_BMT_1[1] = PD->graphics->loadBitmap("images/Node_1/type_1_2", &ERR);
    NODE_BMT_1[2] = PD->graphics->loadBitmap("images/Node_1/type_1_3", &ERR);
    NODE_BMT_1[3] = PD->graphics->loadBitmap("images/Node_1/type_1_4", &ERR);
    NODE_BMT_1[4] = PD->graphics->loadBitmap("images/Node_1/type_1_5", &ERR);
    NODE_BMT_1[5] = PD->graphics->loadBitmap("images/Node_1/type_1_6", &ERR);
    NODE_BMT_1[6] = PD->graphics->loadBitmap("images/Node_1/type_1_7", &ERR);
    NODE_BMT_1[7] = PD->graphics->loadBitmap("images/Node_1/type_1_8", &ERR);
    NODE_BMT_2[0] = PD->graphics->loadBitmap("images/Node_2/type_2_1", &ERR);
    NODE_BMT_2[1] = PD->graphics->loadBitmap("images/Node_2/type_2_2", &ERR);
    NODE_BMT_2[2] = PD->graphics->loadBitmap("images/Node_2/type_2_3", &ERR);
    NODE_BMT_2[3] = PD->graphics->loadBitmap("images/Node_2/type_2_4", &ERR);
    NODE_BMT_2[4] = PD->graphics->loadBitmap("images/Node_2/type_2_5", &ERR);
    NODE_BMT_2[5] = PD->graphics->loadBitmap("images/Node_2/type_2_6", &ERR);
    NODE_BMT_2[6] = PD->graphics->loadBitmap("images/Node_2/type_2_7", &ERR);
    NODE_BMT_2[7] = PD->graphics->loadBitmap("images/Node_2/type_2_8", &ERR);
    
    // make sprites
    const struct playdate_sprite *sprite = PD->sprite;
    PLAYER_SPRITE = sprite->newSprite();
    sprite->setImage(PLAYER_SPRITE, PLAYER_BM, kBitmapUnflipped);
    NODE_1_SPRITE_MASTER = sprite->newSprite();
    sprite->setImage(NODE_1_SPRITE_MASTER, NODE_BMT_1[0], kBitmapUnflipped);
    NODE_2_SPRITE_MASTER = sprite->newSprite();
    sprite->setImage(NODE_2_SPRITE_MASTER, NODE_BMT_2[0], kBitmapUnflipped);
    
    sprite->addSprite(PLAYER_SPRITE);
    PLAYER_X = CENTER_X;
    PLAYER_Y = CENTER_Y;
    sprite->moveTo(PLAYER_SPRITE, PLAYER_X, PLAYER_Y);
    
    // add global effects
    const struct playdate_sound *pdSound = PD->sound;
    CHANNEL = pdSound->channel->newChannel();
    
    HIGH_SHELF = pdSound->effect->twopolefilter->newFilter();
    PD->sound->effect->twopolefilter->setType(HIGH_SHELF, kFilterTypeHighShelf);
    PD->sound->effect->twopolefilter->setFrequency(HIGH_SHELF, 800.0f);
    PD->sound->effect->twopolefilter->setResonance(HIGH_SHELF, 1.0f);
    PD->sound->effect->twopolefilter->setGain(HIGH_SHELF, lerp(-1.0f * MIN_HIGHS, MAX_HIGHS, 0.5f));
    pdSound->channel->addEffect(CHANNEL, (SoundEffect *)HIGH_SHELF);
    
    LOW_SHELF = pdSound->effect->twopolefilter->newFilter();
    PD->sound->effect->twopolefilter->setType(LOW_SHELF, kFilterTypeLowShelf);
    PD->sound->effect->twopolefilter->setFrequency(LOW_SHELF, 300.0f);
    PD->sound->effect->twopolefilter->setResonance(LOW_SHELF, 1.0f);
    PD->sound->effect->twopolefilter->setGain(LOW_SHELF, lerp(-1.0f * MIN_LOWS, MAX_LOWS, 0.5f));
    pdSound->channel->addEffect(CHANNEL, (SoundEffect *)LOW_SHELF);
    
    /*
    DELAY = pdSound->effect->delayline->newDelayLine(SAMPLE_RATE, 1);
    pdSound->effect->delayline->setFeedback(DELAY, 0.9f);
    pdSound->effect->delayline->setLength(DELAY, 35);
    DELAY_OUT = pdSound->effect->delayline->addTap(DELAY, 0);
    pdSound->source->setVolume((SoundSource *)DELAY_OUT, 1.0f, 1.0f);
    pdSound->channel->addSource(CHANNEL, (SoundSource *)DELAY_OUT);
     */
    
    PITCH_FIELD_ID = 0;
    PITCH_FIELD = PITCH_FIELD_SET[PITCH_FIELD_ID];
    NEXT_PITCH_FIELD_CHANGE = CURRENT_TIME + CHANGE_PITCH_FIELD_RATE;
}

#ifdef _WINDLL
__declspec(dllexport)
#endif
int eventHandler(PlaydateAPI* pd, PDSystemEvent event, uint32_t arg)
{
    (void)arg; // arg is currently only used for event = kEventKeyPressed

    if ( event == kEventInit )
    {
        setup(pd);
        
        // get outta here, lua
        pd->system->setUpdateCallback(update, pd);
    }
    
    return 0;
}

static void processInputs(void)
{
    PDButtons current;
    PDButtons pushed;
    PD->system->getButtonState(&current, &pushed, NULL);
    
    if (current & kButtonUp) PLAYER_Y -= 3.0f;
    if (current & kButtonDown) PLAYER_Y += 3.0f;
    if (current & kButtonLeft) PLAYER_X -= 3.0f;
    if (current & kButtonRight) PLAYER_X += 3.0f;
    
    if (PLAYER_X > (float)LCD_COLUMNS) PLAYER_X -= (float)LCD_COLUMNS;
    if (PLAYER_Y > (float)LCD_ROWS) PLAYER_Y -= (float)LCD_ROWS;
    if (PLAYER_X < 0.0f) PLAYER_X += (float)LCD_COLUMNS;
    if (PLAYER_Y < 0.0f) PLAYER_Y += (float)LCD_ROWS;
    
    PD->sprite->moveTo(PLAYER_SPRITE, PLAYER_X, PLAYER_Y);
    
    if (pushed & kButtonA) { makeNode(Strong, PLAYER_X, PLAYER_Y); }
    else if (pushed & kButtonB) { makeNode(Weak, PLAYER_X, PLAYER_Y); }
    
    if (PD->system->isCrankDocked() == 0)
    {
        float change = PD->system->getCrankChange();
        if (change != 0.0f && TIME_VELOCITY < MAX_TIME_VELOCITY + 1 && TIME_VELOCITY > MIN_TIME_VELOCITY - 1)
        {
            // adjust time velocity
            TIME_VELOCITY += change / 720.0f;
            if (TIME_VELOCITY > MAX_TIME_VELOCITY) TIME_VELOCITY = MAX_TIME_VELOCITY;
            if (TIME_VELOCITY < MIN_TIME_VELOCITY) TIME_VELOCITY = MIN_TIME_VELOCITY;
            
            // TODO adjust filters
            float filterAlpha = TIME_VELOCITY/MAX_TIME_VELOCITY;
            PD->sound->effect->twopolefilter->setGain(
                                                      HIGH_SHELF,
                                                      lerp(-1.0f * MIN_HIGHS, MAX_HIGHS, filterAlpha)
                                                      );
            PD->sound->effect->twopolefilter->setGain(
                                                      LOW_SHELF,
                                                      lerp(-1.0f * MIN_LOWS, MAX_LOWS, 1.0f - filterAlpha)
                                                      );
            
            // TODO adjust visuals
        }
    }
}

static int update(void* userdata)
{
    // grab globals
    PD = (PlaydateAPI *)userdata;
    uint32_t newRealTime = PD->sound->getCurrentTime();
    CURRENT_TIME += floorf((float)(newRealTime - LAST_REAL_TIME) * TIME_VELOCITY);
    LAST_REAL_TIME = newRealTime;
    
    processInputs();
    
    if (CURRENT_TIME > NEXT_TOUCH)
    {
        // touch and pulse all live nodes
        for (int i = 0; i < MAX_NODES; i++) { if (NODE_TYPE[i] != Dead) touchNode(i); }
        NEXT_TOUCH = CURRENT_TIME + TOUCH_RATE;
    }
    
    if (CURRENT_TIME > NEXT_PITCH_FIELD_CHANGE)
    {
        PITCH_FIELD_ID = (PITCH_FIELD_ID + 1) % 4;
        PITCH_FIELD = PITCH_FIELD_SET[PITCH_FIELD_ID];
        NEXT_PITCH_FIELD_CHANGE = CURRENT_TIME + CHANGE_PITCH_FIELD_RATE;
    }
    
    // draw stuff
    
    PD->sprite->updateAndDrawSprites();
    
    if (ERR != NULL) PD->system->logToConsole("Error: %s", ERR);

    return 1;
}
