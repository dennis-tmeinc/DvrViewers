/* Things to change.
 * 1. dvrplayer.h
 *    m_playerinfo: change to public 
 *    m_errorstate: change to public 
 *    newstream() : change to public 
 *    m_password  : change to public
 *    m_hVideoKey : change to public
 *    m_channellist:change to public
 * 2. copy inttypedef.h, adpcm.h, adpcm.cpp, xvid.h, xvidenc.h, xvidenc.cpp, videoclip.h, videoclip.cpp,
 *         xvidcore.dll.a to the project folder (ply301)
 * 3. copy xvidcore.dll to the working folder (Debug or Release)
 * 3. Add adpcm.cpp, xvidenc.cpp, videoclip.cpp to the project source file
 * 4. modify dvrplayer.cpp (check for USE_VIDEOCLIP)
 */
