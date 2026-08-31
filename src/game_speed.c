/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file game_speed.c
 *     Control of the game speed.
 * @par Purpose:
 *     Variables and functions keeping game speed and frame rate at required pace.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     10 Feb 2024 - 02 May 2024
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "game_speed.h"

#include <assert.h>
#include <stdlib.h>
#include "bfkeybd.h"
#include "bftime.h"
#include "game.h"
#include "keyboard.h"
#include "swlog.h"

/******************************************************************************/

short frameskip = 0;

// TODO implement separate turns per second, when drawing frames will get separated from game loop
ushort game_num_fps = 16;

ushort fifties_per_gameturn = 3;

ushort render_frames_per_turn = 1;

ushort render_frames_this_turn = 1;

ulong draw_frame = 1;

TbBool frame_advances_state = true;

void render_frames_per_turn_init(void)
{
    const char *env;
    int n;

    env = getenv("SWFX_FRAMES_PER_TURN");
    if (env == NULL)
        return;
    n = atoi(env);
    if ((n < 1) || (n > 8)) {
        LOGERR("SWFX_FRAMES_PER_TURN=%s outside of the 1..8 range, ignored", env);
        return;
    }
    render_frames_per_turn = n;
    LOGSYNC_F("drawing %d frames per game turn", n);
}

/******************************************************************************/

void frameskip_clip(void)
{
    if (frameskip > 512)
        frameskip = 512;
    else if (frameskip < 0)
        frameskip = 0;
}

void frameskip_increase(void)
{
    if (frameskip < 2)
        frameskip++;
    else if (frameskip < 16)
        frameskip += 2;
    else
        frameskip += (frameskip/3);
    frameskip_clip();
    //show_onscreen_msg(game_num_fps+frameskip, "Frame skip %d",frameskip);
}

void frameskip_decrease(void)
{
    if (frameskip <= 2)
        frameskip--;
    else if (frameskip <= 16)
        frameskip -= 2;
    else
        frameskip -= (frameskip/4);
    frameskip_clip();
    //show_onscreen_msg(game_num_fps+frameskip, "Frame skip %d",frameskip);
}

ubyte get_speed_control_inputs(void)
{
    ubyte did_inp;

    did_inp = GINPUT_NONE;
    if (is_gamekey_pressed(GKey_GAMESPEED_INC))
    {
        clear_gamekey_pressed(GKey_GAMESPEED_INC);
        frameskip_increase();
        did_inp |= GINPUT_DIRECT;
    }
    if (is_gamekey_pressed(GKey_GAMESPEED_DEC))
    {
        clear_gamekey_pressed(GKey_GAMESPEED_DEC);
        frameskip_decrease();
        did_inp |= GINPUT_DIRECT;
    }
    return did_inp;
}

void wait_next_gameturn(void)
{
    static TbClockMSec last_loop_time = 0;
    static ushort subframe = 0;
    static ushort last_nframes = 1;

    TbClockMSec curr_time = LbTimerClock();
    TbClockMSec sleep_end;
    TbClockMSec turn_len, period;
    ushort nframes;

    nframes = render_frames_this_turn;
    if (nframes < 1)
        nframes = 1;
    // The pace changed, ie. a mission was left for a menu; restart the split
    // so that the frames of a turn keep adding up to one turn
    if (nframes != last_nframes) {
        subframe = 0;
        last_nframes = nframes;
    }
    turn_len = 1000/game_num_fps;
    // Spread the turn over its frames without losing the division remainder,
    // so that a turn still lasts exactly as long as it used to
    period = (turn_len * (subframe + 1)) / nframes - (turn_len * subframe) / nframes;
    subframe++;
    if (subframe >= nframes)
        subframe = 0;
    if (period < 1)
        period = 1;

    if (frameskip == 0)
        sleep_end = last_loop_time + period;
    else
        sleep_end = curr_time;
    // If we missed the normal sleep target (ie. there was a slowdown), reset the value and do not sleep
    if ((sleep_end < curr_time) || (sleep_end > curr_time + turn_len)) {
        LOGNO("missed FPS target, last frame time %ld too far from current %ld",
          (ulong)sleep_end, (ulong)curr_time);
        sleep_end = curr_time;
        subframe = 0;
    }
    LbSleepUntil(sleep_end);
    last_loop_time = sleep_end;
}

/**
 * Checks if the game screen needs redrawing.
 */
TbBool display_needs_redraw_this_turn(void)
{
    if ( (frameskip == 0) || ((gameturn % frameskip) == 0))
        return true;
    return false;
}

void update_tick_time(void)
{
    ulong tick_time = clock();
    tick_time = tick_time / 100;
    curr_tick_time = tick_time;
    if (tick_time != prev_tick_time)
    {
        ulong tmp;
        tmp = gameturn - prev_gameturn;
        prev_gameturn = gameturn;
        turns_delta = tmp;
    }
    if ( turns_delta != 0 ) {
        fifties_per_gameturn = 800 / turns_delta;
    } else {
        fifties_per_gameturn = 50;
    }
    if ( in_network_game )
        fifties_per_gameturn = 80;
    if ( fifties_per_gameturn > 400 )
        fifties_per_gameturn = 400;
    prev_tick_time = curr_tick_time;
}

/******************************************************************************/
