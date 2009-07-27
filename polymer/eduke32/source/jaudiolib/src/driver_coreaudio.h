/*
 Copyright (C) 2009 Jonathon Fowler <jf@jonof.id.au>
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 
 See the GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 
 */

int32_t CoreAudioDrv_GetError(void);
const char *CoreAudioDrv_ErrorString( int32_t ErrorNumber );
int32_t CoreAudioDrv_Init(int32_t mixrate, int32_t numchannels, int32_t samplebits, void * initdata);
void CoreAudioDrv_Shutdown(void);
int32_t CoreAudioDrv_BeginPlayback(char *BufferStart, int32_t BufferSize,
             int32_t NumDivisions, void ( *CallBackFunc )( void ) );
void CoreAudioDrv_StopPlayback(void);
void CoreAudioDrv_Lock(void);
void CoreAudioDrv_Unlock(void);