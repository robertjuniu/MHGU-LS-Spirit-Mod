#ifndef MHGU_LS_DIAGNOSTICS_H
#define MHGU_LS_DIAGNOSTICS_H
/* Public builds exclude prototype debug calls and their argument evaluation. */
#ifdef IAI_RELEASE
#define iai_log_gp(...) ((void)0)
#define iai_log_cuts(...) ((void)0)
#define iai_log_combo_first(...) ((void)0)
#define iai_log_combo_second(...) ((void)0)
#define iai_log_reward_level(...) ((void)0)
#define iai_log_reward_valor(...) ((void)0)
#define iai_log_reward_delay(...) ((void)0)
#define iai_log_yaw_exit(...) ((void)0)
#define iai_log_alt_request(...) ((void)0)
#define iai_log_alt_accepted(...) ((void)0)
#define iai_log_blend_skip(...) ((void)0)
#define iai_log_chain_buffer(...) ((void)0)
#define iai_log_chain_commit(...) ((void)0)
#define iai_log_window_cue(...) ((void)0)
#define iai_log_roll_entry(...) ((void)0)
#define iai_log_audio_sheath(...) ((void)0)
#define iai_log_audio_draw(...) ((void)0)
#define iai_log_audio_cut(...) ((void)0)
#define iai_log_audio_missing(...) ((void)0)
#define iai_log_native_end(...) ((void)0)
#define iai_log_cancel7(...) ((void)0)
#define iai_log_span(text,size) ((void)(text))
#endif
#endif
