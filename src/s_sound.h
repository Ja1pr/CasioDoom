//-----------------------------------------------------------------------------
//
// CasioDoom: the calculator has no speaker. Every sound call compiles to
// nothing, so the game code can keep its S_StartSound() calls untouched.
//
//-----------------------------------------------------------------------------

#ifndef __S_SOUND__
#define __S_SOUND__

#define S_Init(sfx,mus)			((void)0)
#define S_Start()			((void)0)
#define S_StartSound(origin,id)		((void)0)
#define S_StartSoundAtVolume(o,id,vol)	((void)0)
#define S_StopSound(origin)		((void)0)
#define S_StartMusic(id)		((void)0)
#define S_ChangeMusic(id,looping)	((void)0)
#define S_StopMusic()			((void)0)
#define S_PauseSound()			((void)0)
#define S_ResumeSound()			((void)0)
#define S_UpdateSounds(listener)	((void)0)
#define S_SetMusicVolume(v)		((void)0)
#define S_SetSfxVolume(v)		((void)0)

#endif
