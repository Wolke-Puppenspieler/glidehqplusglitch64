/*
* Glide64 - Glide video plugin for Nintendo 64 emulators.
* Copyright (c) 2002  Dave2001
* Copyright (c) 2003-2009  Sergey 'Gonetz' Lipski
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

//****************************************************************
//
// Glide64 - Glide Plugin for Nintendo 64 emulators
// Project started on December 29th, 2001
//
// Authors:
// Dave2001, original author, founded the project in 2001, left it in 2002
// Gugaman, joined the project in 2002, left it in 2002
// Sergey 'Gonetz' Lipski, joined the project in 2002, main author since fall of 2002
// Hiroshi 'KoolSmoky' Morii, joined the project in 2007
//
//****************************************************************
//
// To modify Glide64:
// * Write your name and (optional)email, commented by your work, so I know who did it, and so that you can find which parts you modified when it comes time to send it to me.
// * Do NOT send me the whole project or file that you modified.  Take out your modified code sections, and tell me where to put them.  If people sent the whole thing, I would have many different versions, but no idea how to combine them all.
//
//****************************************************************

#include "Gfx #1.3.h"
#include <wx/fileconf.h>
#include <wx/wfstream.h>
#include "Util.h"
#include "3dmath.h"
#include "Debugger.h"
#include "Combine.h"
#include "TexCache.h"
#include "CRC.h"
#include "FBtoScreen.h"
#include "DepthBufferRender.h"

#ifdef TEXTURE_FILTER // Hiroshi Morii <koolsmoky@users.sourceforge.net>
#include <stdarg.h>
int  ghq_dmptex_toggle_key = 0;
#endif

#define G64_VERSION "'Napalm WX' WIP "
#define RELTIME "Date: " __DATE__// " Time: " __TIME__

#ifdef EXT_LOGGING
std::ofstream extlog;
#endif

#ifdef LOGGING
std::ofstream loga;
#endif

#ifdef RDP_LOGGING
int log_open = FALSE;
std::ofstream rdp_log;
#endif

#ifdef RDP_ERROR_LOG
int elog_open = FALSE;
std::ofstream rdp_err;
#endif

GFX_INFO gfx;
wxWindow * GFXWindow = NULL;

int to_fullscreen = FALSE;
int fullscreen = FALSE;
int romopen = FALSE;
GrContext_t gfx_context = 0;
int debugging = FALSE;
int exception = FALSE;

int evoodoo = 0;
int ev_fullscreen = 0;

#ifdef __WINDOWS__
#define WINPROC_OVERRIDE
#endif

#ifdef WINPROC_OVERRIDE
LRESULT CALLBACK WndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
WNDPROC oldWndProc = NULL;
WNDPROC myWndProc = NULL;
#endif

#ifdef ALTTAB_FIX
HHOOK hhkLowLevelKybd = NULL;
LRESULT CALLBACK LowLevelKeyboardProc(int nCode,
                                      WPARAM wParam, LPARAM lParam);
#endif

#ifdef PERFORMANCE
int64 perf_cur;
int64 perf_next;
#endif

#ifdef FPS
wxDateTime fps_last;
wxDateTime fps_next;
float      fps = 0.0f;
wxUint32   fps_count = 0;

wxUint32   vi_count = 0;
float      vi = 0.0f;

wxUint32   region = 0;

float      ntsc_percent = 0.0f;
float      pal_percent = 0.0f;

#endif

// Resolutions, MUST be in the correct order (SST1VID.H)
wxUint32 resolutions[0x18][2] = {
  { 320, 200 },
  { 320, 240 },
  { 400, 256 },
  { 512, 384 },
  { 640, 200 },
  { 640, 350 },
  { 640, 400 },
  { 640, 480 },
  { 800, 600 },
  { 960, 720 },
  { 856, 480 },
  { 512, 256 },
  { 1024, 768 },
  { 1280, 1024 },
  { 1600, 1200 },
  { 400, 300 },

  // 0x10
  { 1152, 864 },
  { 1280, 960 },
  { 1600, 1024 },
  { 1792, 1344 },
  { 1856, 1392 },
  { 1920, 1440 },
  { 2048, 1536 },
  { 2048, 2048 }
};

// ref rate
// 60=0x0, 70=0x1, 72=0x2, 75=0x3, 80=0x4, 90=0x5, 100=0x6, 85=0x7, 120=0x8, none=0xff

unsigned int BMASK = 0x7FFFFF;
// Reality display processor structure
RDP rdp;

SETTINGS settings = { FALSE, 640, 480, GR_RESOLUTION_640x480, 0 };

HOTKEY_INFO hotkey_info;

VOODOO voodoo = {0, 0, 0, 0,
                 0, 0, 0, 0,
                 0, 0, 0, 0
                };

GrTexInfo fontTex;
GrTexInfo cursorTex;
wxUint32   offset_font = 0;
wxUint32   offset_cursor = 0;
wxUint32   offset_textures = 0;
wxUint32   offset_texbuf1 = 0;

int    capture_screen = 0;
wxString capture_path;

wxString pluginPath;
wxString iniPath;
wxString iniName;

/******************************************************************
   NOTE: THIS HAS BEEN ADDED FOR MUPEN64PLUS AND IS NOT PART OF THE
         ORIGINAL SPEC
  Function: SetConfigDir
  Purpose:  To pass the location where config files should be read/
            written to.
  input:    path to config directory
  output:   none
*******************************************************************/
EXPORT void CALL SetConfigDir(char *configDir)
{
  wxString dirName(configDir, wxConvUTF8);
  wxString path = wxPathOnly(dirName);
  if (wxDirExists(path))
  {
    iniName = path + wxT("/Glide64.ini");
    iniPath = path;
  }
}

static void PluginPath()
{
  wxDynamicLibraryDetailsArray dlls = wxDynamicLibrary::ListLoaded();
  const size_t count = dlls.GetCount();
  for ( size_t n = 0; n < count; ++n )
  {
    const wxDynamicLibraryDetails& details = dlls[n];
    if (details.GetName().Find(wxT("Glide64")) != wxNOT_FOUND)
    {
      wxFileName libname(details.GetPath());
      pluginPath = libname.GetPath();
      return;
    }
  }
  pluginPath = wxGetCwd() + _T("/Plugin"); //if ListLoaded is not supported by OS use default path
}

void _ChangeSize ()
{
  rdp.scale_1024 = settings.scr_res_x / 1024.0f;
  rdp.scale_768 = settings.scr_res_y / 768.0f;

//  float res_scl_x = (float)settings.res_x / 320.0f;
  float res_scl_y = (float)settings.res_y / 240.0f;

  wxUint32 scale_x = *gfx.VI_X_SCALE_REG & 0xFFF;
  if (!scale_x) return;
  wxUint32 scale_y = *gfx.VI_Y_SCALE_REG & 0xFFF;
  if (!scale_y) return;

  float fscale_x = (float)scale_x / 1024.0f;
  float fscale_y = (float)scale_y / 2048.0f;

  wxUint32 dwHStartReg = *gfx.VI_H_START_REG;
  wxUint32 dwVStartReg = *gfx.VI_V_START_REG;

  wxUint32 hstart = dwHStartReg >> 16;
  wxUint32 hend = dwHStartReg & 0xFFFF;

  // dunno... but sometimes this happens
  if (hend == hstart) hend = (int)(*gfx.VI_WIDTH_REG / fscale_x);

  wxUint32 vstart = dwVStartReg >> 16;
  wxUint32 vend = dwVStartReg & 0xFFFF;

  rdp.vi_width = (hend - hstart) * fscale_x;
  rdp.vi_height = (vend - vstart) * fscale_y * 1.0126582f;
  float aspect = (settings.adjust_aspect && (fscale_y > fscale_x) && (rdp.vi_width > rdp.vi_height)) ? fscale_x/fscale_y : 1.0f;

#ifdef LOGGING
  sprintf (out_buf, "hstart: %d, hend: %d, vstart: %d, vend: %d\n", hstart, hend, vstart, vend);
  LOG (out_buf);
  sprintf (out_buf, "size: %d x %d\n", (int)rdp.vi_width, (int)rdp.vi_height);
  LOG (out_buf);
#endif

  rdp.scale_x = (float)settings.res_x / rdp.vi_width;
  if (region > 0 && settings.pal230)
  {
    // odd... but pal games seem to want 230 as height...
    rdp.scale_y = res_scl_y * (230.0f / rdp.vi_height)  * aspect;
  }
  else
  {
    rdp.scale_y = (float)settings.res_y / rdp.vi_height * aspect;
  }
  //  rdp.offset_x = settings.offset_x * res_scl_x;
  //  rdp.offset_y = settings.offset_y * res_scl_y;
  //rdp.offset_x = 0;
  //  rdp.offset_y = 0;
  rdp.offset_y = ((float)settings.res_y - rdp.vi_height * rdp.scale_y) * 0.5f;
  if (((wxUint32)rdp.vi_width <= (*gfx.VI_WIDTH_REG)/2) && (rdp.vi_width > rdp.vi_height))
    rdp.scale_y *= 0.5f;

  rdp.scissor_o.ul_x = 0;
  rdp.scissor_o.ul_y = 0;
  rdp.scissor_o.lr_x = (wxUint32)rdp.vi_width;
  rdp.scissor_o.lr_y = (wxUint32)rdp.vi_height;

  rdp.update |= UPDATE_VIEWPORT | UPDATE_SCISSOR;
}

void ChangeSize ()
{
  if (debugging)
  {
    _ChangeSize ();
    return;
  }
  switch (settings.aspectmode)
  {
  case 0: //4:3
    if (settings.scr_res_x >= settings.scr_res_y * 1.333333f) {
      settings.res_y = settings.scr_res_y;
      settings.res_x = (wxUint32)(settings.res_y * 1.333334f);
    } else {
      settings.res_x = settings.scr_res_x;
      settings.res_y = (wxUint32)(settings.res_x / 1.333334f);
    }
    break;
  case 1: //16:9
    if (settings.scr_res_x >= settings.scr_res_y * 1.777777f) {
      settings.res_y = settings.scr_res_y;
      settings.res_x = (wxUint32)(settings.res_y * 1.777778f);
    } else {
      settings.res_x = settings.scr_res_x;
      settings.res_y = (wxUint32)(settings.res_x / 1.777778f);
    }
    break;
  default: //stretch
    settings.res_x = settings.scr_res_x;
    settings.res_y = settings.scr_res_y;
  }
  _ChangeSize ();
  rdp.offset_x = (settings.scr_res_x - settings.res_x) / 2.0f;
  float offset_y = (settings.scr_res_y - settings.res_y) / 2.0f;
  settings.res_x += (wxUint32)rdp.offset_x;
  settings.res_y += (wxUint32)offset_y;
  rdp.offset_y += offset_y;
  //	settings.res_x = settings.scr_res_x;
  //	settings.res_y = settings.scr_res_y;
}

static wxConfigBase * OpenIni()
{
  wxConfigBase * ini = wxConfigBase::Get(false);
  if (!ini)
  {
    if (iniName.IsEmpty())
      iniName = pluginPath + wxT("/Glide64.ini");
    if (wxFileExists(iniName))
    {
      wxFileInputStream is(iniName);
      wxFileConfig * fcfg = new wxFileConfig(is, wxConvISO8859_1);
      wxConfigBase::Set(fcfg);
      ini = fcfg;
    }
  }
  if (!ini)
    wxMessageBox(_T("Can not find ini file! Plugin will not run properly."), _T("File not found"), wxOK|wxICON_EXCLAMATION);
  return ini;
}

void ReadSettings ()
{
  //  LOG("ReadSettings\n");
  wxConfigBase * ini = OpenIni();
  if (!ini || !ini->HasGroup(_T("/SETTINGS")))
    return;
  ini->SetPath(_T("/SETTINGS"));

  settings.card_id = ini->Read(_T("card_id"), 0l);
  settings.lang_id = ini->Read(_T("lang_id"), wxLANGUAGE_ENGLISH_US);
  settings.res_data = (wxUint32)ini->Read(_T("resolution"), 7);
  if (settings.res_data >= 24) settings.res_data = 12;
  settings.scr_res_x = settings.res_x = resolutions[settings.res_data][0];
  settings.scr_res_y = settings.res_y = resolutions[settings.res_data][1];
  settings.vsync = ini->Read(_T("vsync"), 0l);
  settings.ssformat = (wxUint8)ini->Read(_T("ssformat"), 0l);
  settings.show_fps = (wxUint8)ini->Read(_T("show_fps"), 0l);
  settings.clock = ini->Read(_T("clock"), 0l);
  settings.clock_24_hr = ini->Read(_T("clock_24_hr"), 0l);
  settings.advanced_options = ini->Read(_T("advanced_options"), 0l);
  settings.texenh_options = ini->Read(_T("texenh_options"), 0l);

  settings.wrpResolution = ini->Read(_T("wrpResolution"), 0l);
  settings.wrpVRAM = ini->Read(_T("wrpVRAM"), 0l);
  settings.wrpFBO = ini->Read(_T("wrpFBO"), 0l);
  settings.wrpAnisotropic = ini->Read(_T("wrpAnisotropic"), 0l);

#ifndef _ENDUSER_RELEASE_
  settings.autodetect_ucode = ini->Read(_T("autodetect_ucode"), 1);
  settings.ucode = ini->Read(_T("ucode"), 2);
  settings.wireframe = ini->Read(_T("wireframe"), 0l);
  settings.wfmode = ini->Read(_T("wfmode"), 1);
  settings.logging = ini->Read(_T("logging"), 0l);
  settings.log_clear = ini->Read(_T("log_clear"), 0l);
  settings.run_in_window = ini->Read(_T("run_in_window"), 0l);
  settings.elogging = ini->Read(_T("elogging"), 0l);
  settings.filter_cache = ini->Read(_T("filter_cache"), 0l);
  settings.unk_as_red = ini->Read(_T("unk_as_red"), 0l);
  settings.log_unk = ini->Read(_T("log_unk"), 0l);
  settings.unk_clear = ini->Read(_T("unk_clear"), 0l);
#else
  settings.autodetect_ucode = TRUE;
  settings.ucode = 2;
  settings.wireframe = FALSE;
  settings.wfmode = 0;
  settings.logging = FALSE;
  settings.log_clear = FALSE;
  settings.run_in_window = FALSE;
  settings.elogging = FALSE;
  settings.filter_cache = FALSE;
  settings.unk_as_red = FALSE;
  settings.log_unk = FALSE;
  settings.unk_clear = FALSE;
#endif

#ifdef TEXTURE_FILTER
  settings.ghq_fltr = (wxUint8)ini->Read(_T("ghq_fltr"), 0l);
  settings.ghq_cmpr = (wxUint8)ini->Read(_T("ghq_cmpr"), 0l);
  settings.ghq_enht = (wxUint8)ini->Read(_T("ghq_enht"), 0l);
  settings.ghq_hirs = (wxUint8)ini->Read(_T("ghq_hirs"), 0l);
  settings.ghq_enht_cmpr  = ini->Read(_T("ghq_enht_cmpr"), 0l);
  settings.ghq_enht_tile = ini->Read(_T("ghq_enht_tile"), 0l);
  settings.ghq_enht_f16bpp = ini->Read(_T("ghq_enht_f16bpp"), 0l);
  settings.ghq_enht_gz  = ini->Read(_T("ghq_enht_gz"), 1L);
  settings.ghq_enht_nobg  = ini->Read(_T("ghq_enht_nobg"), 0l);
  settings.ghq_hirs_cmpr  = ini->Read(_T("ghq_hirs_cmpr"), 0l);
  settings.ghq_hirs_tile = ini->Read(_T("ghq_hirs_tile"), 0l);
  settings.ghq_hirs_f16bpp = ini->Read(_T("ghq_hirs_f16bpp"), 0l);
  settings.ghq_hirs_gz  = ini->Read(_T("ghq_hirs_gz"), 1);
  settings.ghq_hirs_altcrc  = ini->Read(_T("ghq_hirs_altcrc"), 1);
  settings.ghq_cache_save = ini->Read(_T("ghq_cache_save"), 1);
  settings.ghq_cache_size = ini->Read(_T("ghq_cache_size"), 0l);
  settings.ghq_hirs_let_texartists_fly = ini->Read(_T("ghq_hirs_let_texartists_fly"), 0l);
  settings.ghq_hirs_dump = ini->Read(_T("ghq_hirs_dump"), 0l);
#endif
}

void ReadSpecialSettings (const char * name)
{
  //  char buf [256];
  //  sprintf(buf, "ReadSpecialSettings. Name: %s\n", name);
  //  LOG(buf);
  settings.hacks = 0;

  //detect games which require special hacks
  if (strstr(name, (const char *)"ZELDA") || strstr(name, (const char *)"MASK"))
    settings.hacks |= hack_Zelda;
  else if (strstr(name, (const char *)"ROADSTERS TROPHY"))
    settings.hacks |= hack_Zelda;
  else if (strstr(name, (const char *)"Diddy Kong Racing"))
    settings.hacks |= hack_Diddy;
  else if (strstr(name, (const char *)"Tonic Trouble"))
    settings.hacks |= hack_Tonic;
  else if (strstr(name, (const char *)"All") && strstr(name, (const char *)"Star") && strstr(name, (const char *)"Baseball"))
    settings.hacks |= hack_ASB;
  else if (strstr(name, (const char *)"Beetle") || strstr(name, (const char *)"BEETLE") || strstr(name, (const char *)"HSV"))
    settings.hacks |= hack_BAR;
  else if (strstr(name, (const char *)"I S S 64") || strstr(name, (const char *)"J WORLD SOCCER3") || strstr(name, (const char *)"PERFECT STRIKER"))
    settings.hacks |= hack_ISS64;
  else if (strstr(name, (const char *)"MARIOKART64"))
    settings.hacks |= hack_MK64;
  else if (strstr(name, (const char *)"NITRO64"))
    settings.hacks |= hack_WCWnitro;
  else if (strstr(name, (const char *)"CHOPPER_ATTACK") || strstr(name, (const char *)"WILD CHOPPERS"))
    settings.hacks |= hack_Chopper;
  else if (strstr(name, (const char *)"Resident Evil II") || strstr(name, (const char *)"BioHazard II"))
    settings.hacks |= hack_RE2;
  else if (strstr(name, (const char *)"YOSHI STORY"))
    settings.hacks |= hack_Yoshi;
  else if (strstr(name, (const char *)"F-Zero X") || strstr(name, (const char *)"F-ZERO X"))
    settings.hacks |= hack_Fzero;
  else if (strstr(name, (const char *)"PAPER MARIO") || strstr(name, (const char *)"MARIO STORY"))
    settings.hacks |= hack_PMario;
  else if (strstr(name, (const char *)"TOP GEAR RALLY 2"))
    settings.hacks |= hack_TGR2;
  else if (strstr(name, (const char *)"TOP GEAR RALLY"))
    settings.hacks |= hack_TGR;
  else if (strstr(name, (const char *)"Top Gear Hyper Bike"))
    settings.hacks |= hack_Hyperbike;
  else if (strstr(name, (const char *)"Killer Instinct Gold") || strstr(name, (const char *)"KILLER INSTINCT GOLD"))
    settings.hacks |= hack_KI;
  else if (strstr(name, (const char *)"Knockout Kings 2000"))
    settings.hacks |= hack_Knockout;
  else if (strstr(name, (const char *)"LEGORacers"))
    settings.hacks |= hack_Lego;
  else if (strstr(name, (const char *)"OgreBattle64"))
    settings.hacks |= hack_Ogre64;
  else if (strstr(name, (const char *)"Pilot Wings64"))
    settings.hacks |= hack_Pilotwings;
  else if (strstr(name, (const char *)"Supercross"))
    settings.hacks |= hack_Supercross;
  else if (strstr(name, (const char *)"STARCRAFT 64"))
    settings.hacks |= hack_Starcraft;
  else if (strstr(name, (const char *)"BANJO KAZOOIE 2") || strstr(name, (const char *)"BANJO TOOIE"))
    settings.hacks |= hack_Banjo2;
  else if (strstr(name, (const char *)"FIFA: RTWC 98") || strstr(name, (const char *)"RoadToWorldCup98"))
    settings.hacks |= hack_Fifa98;
  else if (strstr(name, (const char *)"Mega Man 64") || strstr(name, (const char *)"RockMan Dash"))
    settings.hacks |= hack_Megaman;
  else if (strstr(name, (const char *)"MISCHIEF MAKERS") || strstr(name, (const char *)"TROUBLE MAKERS"))
    settings.hacks |= hack_Makers;
  else if (strstr(name, (const char *)"GOLDENEYE"))
    settings.hacks |= hack_GoldenEye;

  wxString groupName = wxT("/");
  groupName += wxString::FromAscii(name);
  wxConfigBase * ini = OpenIni();
  if (!ini || !ini->HasGroup(groupName))
    return;
  ini->SetPath(groupName);

  ini->Read(_T("alt_tex_size"), &(settings.alt_tex_size));
  ini->Read(_T("use_sts1_only"), &(settings.use_sts1_only));
  if (ini->Read(_T("PPL"), -1) == 1) settings.hacks |= hack_PPL;
  ini->Read(_T("force_calc_sphere"), &(settings.force_calc_sphere));
  ini->Read(_T("correct_viewport"), &(settings.correct_viewport));
  ini->Read(_T("increase_texrect_edge"), &(settings.increase_texrect_edge));
  ini->Read(_T("decrease_fillrect_edge"), &(settings.decrease_fillrect_edge));
  if (ini->Read(_T("texture_correction"), -1) == 0) settings.texture_correction = 0;
  else settings.texture_correction = 1;
  if (ini->Read(_T("pal230"), -1) == 1) settings.pal230 = 1;
  else settings.pal230 = 0;
  ini->Read(_T("stipple_mode"), &(settings.stipple_mode));
  int stipple_pattern = ini->Read(_T("stipple_pattern"), -1);
  if (stipple_pattern > 0) settings.stipple_pattern = (wxUint32)stipple_pattern;
  ini->Read(_T("force_microcheck"), &(settings.force_microcheck));
  ini->Read(_T("force_quad3d"), &(settings.force_quad3d));
  ini->Read(_T("clip_zmin"), &(settings.clip_zmin));
  ini->Read(_T("clip_zmax"), &(settings.clip_zmax));
  ini->Read(_T("fast_crc"), &(settings.fast_crc));
  ini->Read(_T("adjust_aspect"), &(settings.adjust_aspect), 1);
  ini->Read(_T("zmode_compare_less"), &(settings.zmode_compare_less));
  ini->Read(_T("n64_z_scale"), &(settings.n64_z_scale));
  if (settings.n64_z_scale)
    ZLUT_init();

  //frame buffer
  int optimize_texrect = ini->Read(_T("optimize_texrect"), -1);
  int ignore_aux_copy = ini->Read(_T("ignore_aux_copy"), -1);
  int hires_buf_clear = ini->Read(_T("hires_buf_clear"), -1);
  int read_alpha = ini->Read(_T("fb_read_alpha"), -1);
  int useless_is_useless = ini->Read(_T("useless_is_useless"), -1);

  if (optimize_texrect > 0) settings.frame_buffer |= fb_optimize_texrect;
  else if (optimize_texrect == 0) settings.frame_buffer &= ~fb_optimize_texrect;
  if (ignore_aux_copy > 0) settings.frame_buffer |= fb_ignore_aux_copy;
  else if (ignore_aux_copy == 0) settings.frame_buffer &= ~fb_ignore_aux_copy;
  if (hires_buf_clear > 0) settings.frame_buffer |= fb_hwfbe_buf_clear;
  else if (hires_buf_clear == 0) settings.frame_buffer &= ~fb_hwfbe_buf_clear;
  if (read_alpha > 0) settings.frame_buffer |= fb_read_alpha;
  else if (read_alpha == 0) settings.frame_buffer &= ~fb_read_alpha;
  if (useless_is_useless > 0) settings.frame_buffer |= fb_useless_is_useless;
  else settings.frame_buffer &= ~fb_useless_is_useless;

  //  if (settings.custom_ini)
  {
    ini->Read(_T("filtering"), &(settings.filtering));
    ini->Read(_T("fog"), &(settings.fog));
    ini->Read(_T("buff_clear"), &(settings.buff_clear));
    ini->Read(_T("swapmode"), &(settings.swapmode));
    ini->Read(_T("aspect"), &(settings.aspectmode));
    if (settings.lodmode == 0)
      ini->Read(_T("lodmode"), &(settings.lodmode));
    int resolution;
    if (ini->Read(_T("resolution"), &resolution))
    {
      settings.res_data = (wxUint32)resolution;
      if (settings.res_data >= 0x18) settings.res_data = 12;
      settings.scr_res_x = settings.res_x = resolutions[settings.res_data][0];
      settings.scr_res_y = settings.res_y = resolutions[settings.res_data][1];
    }

    //frame buffer
    int smart_read = ini->Read(_T("fb_smart"), -1);
    int hires = ini->Read(_T("fb_hires"), -1);
    int read_always = ini->Read(_T("fb_read_always"), -1);
    int read_back_to_screen = ini->Read(_T("read_back_to_screen"), -1);
    int cpu_write_hack = ini->Read(_T("detect_cpu_write"), -1);
    int get_fbinfo = ini->Read(_T("fb_get_info"), -1);
    int depth_render = ini->Read(_T("fb_render"), -1);

    if (smart_read > 0) settings.frame_buffer |= fb_emulation;
    else if (smart_read == 0) settings.frame_buffer &= ~fb_emulation;
    if (hires > 0) settings.frame_buffer |= fb_hwfbe;
    else if (hires == 0) settings.frame_buffer &= ~fb_hwfbe;
    if (read_always > 0) settings.frame_buffer |= fb_ref;
    else if (read_always == 0) settings.frame_buffer &= ~fb_ref;
    if (read_back_to_screen == 1) settings.frame_buffer |= fb_read_back_to_screen;
    else if (read_back_to_screen == 2) settings.frame_buffer |= fb_read_back_to_screen2;
    else if (read_back_to_screen == 0) settings.frame_buffer &= ~(fb_read_back_to_screen|fb_read_back_to_screen2);
    if (cpu_write_hack > 0) settings.frame_buffer |= fb_cpu_write_hack;
    else if (cpu_write_hack == 0) settings.frame_buffer &= ~fb_cpu_write_hack;
    if (get_fbinfo > 0) settings.frame_buffer |= fb_get_info;
    else if (get_fbinfo == 0) settings.frame_buffer &= ~fb_get_info;
    if (depth_render > 0) settings.frame_buffer |= fb_depth_render;
    else if (depth_render == 0) settings.frame_buffer &= ~fb_depth_render;
    settings.frame_buffer |= fb_motionblur;
  }
  settings.flame_corona = (settings.hacks & hack_Zelda) && !fb_depth_render_enabled;
}

void WriteSettings (bool saveEmulationSettings)
{
  wxConfigBase * ini = OpenIni();
  if (!ini || !ini->HasGroup(_T("/SETTINGS")))
    return;
  ini->SetPath(_T("/SETTINGS"));

  ini->Write(_T("card_id"), settings.card_id);
  ini->Write(_T("lang_id"), settings.lang_id);
  ini->Write(_T("resolution"), (int)settings.res_data);
  ini->Write(_T("ssformat"), settings.ssformat);
  ini->Write(_T("vsync"), settings.vsync);
  ini->Write(_T("show_fps"), settings.show_fps);
  ini->Write(_T("clock"), settings.clock);
  ini->Write(_T("clock_24_hr"), settings.clock_24_hr);
  ini->Write(_T("advanced_options"), settings.advanced_options);
  ini->Write(_T("texenh_options"), settings.texenh_options);

  ini->Write(_T("wrpResolution"), settings.wrpResolution);
  ini->Write(_T("wrpVRAM"), settings.wrpVRAM);
  ini->Write(_T("wrpFBO"), settings.wrpFBO);
  ini->Write(_T("wrpAnisotropic"), settings.wrpAnisotropic);

#ifndef _ENDUSER_RELEASE_
  ini->Write(_T("autodetect_ucode"), settings.autodetect_ucode);
  ini->Write(_T("ucode"), (int)settings.ucode);
  ini->Write(_T("wireframe"), settings.wireframe);
  ini->Write(_T("wfmode"), settings.wfmode);
  ini->Write(_T("logging"), settings.logging);
  ini->Write(_T("log_clear"), settings.log_clear);
  ini->Write(_T("run_in_window"), settings.run_in_window);
  ini->Write(_T("elogging"), settings.elogging);
  ini->Write(_T("filter_cache"), settings.filter_cache);
  ini->Write(_T("unk_as_red"), settings.unk_as_red);
  ini->Write(_T("log_unk"), settings.log_unk);
  ini->Write(_T("unk_clear"), settings.unk_clear);
#endif //_ENDUSER_RELEASE_

#ifdef TEXTURE_FILTER
  ini->Write(_T("ghq_fltr"), settings.ghq_fltr);
  ini->Write(_T("ghq_cmpr"), settings.ghq_cmpr);
  ini->Write(_T("ghq_enht"), settings.ghq_enht);
  ini->Write(_T("ghq_hirs"), settings.ghq_hirs);
  ini->Write(_T("ghq_enht_cmpr"), settings.ghq_enht_cmpr);
  ini->Write(_T("ghq_enht_tile"), settings.ghq_enht_tile);
  ini->Write(_T("ghq_enht_f16bpp"), settings.ghq_enht_f16bpp);
  ini->Write(_T("ghq_enht_gz"), settings.ghq_enht_gz);
  ini->Write(_T("ghq_enht_nobg"), settings.ghq_enht_nobg);
  ini->Write(_T("ghq_hirs_cmpr"), settings.ghq_hirs_cmpr);
  ini->Write(_T("ghq_hirs_tile"), settings.ghq_hirs_tile);
  ini->Write(_T("ghq_hirs_f16bpp"), settings.ghq_hirs_f16bpp);
  ini->Write(_T("ghq_hirs_gz"), settings.ghq_hirs_gz);
  ini->Write(_T("ghq_hirs_altcrc"), settings.ghq_hirs_altcrc);
  ini->Write(_T("ghq_cache_save"), settings.ghq_cache_save);
  ini->Write(_T("ghq_cache_size"), settings.ghq_cache_size);
  ini->Write(_T("ghq_hirs_let_texartists_fly"), settings.ghq_hirs_let_texartists_fly);
  ini->Write(_T("ghq_hirs_dump"), settings.ghq_hirs_dump);
#endif

  if (saveEmulationSettings)
  {
    if (romopen)
    {
      wxString S = _T("/");
      ini->SetPath(S+rdp.RomName);
    }
    else
      ini->SetPath(_T("/DEFAULT"));
    ini->Write(_T("filtering"), settings.filtering);
    ini->Write(_T("fog"), settings.fog);
    ini->Write(_T("buff_clear"), settings.buff_clear);
    ini->Write(_T("swapmode"), settings.swapmode);
    ini->Write(_T("lodmode"), settings.lodmode);
    ini->Write(_T("aspect"), settings.aspectmode);

    ini->Write(_T("fb_read_always"), settings.frame_buffer&fb_ref ? 1 : 0l);
    ini->Write(_T("fb_smart"), settings.frame_buffer & fb_emulation ? 1 : 0l);
    //    ini->Write("motionblur", settings.frame_buffer & fb_motionblur ? 1 : 0);
    ini->Write(_T("fb_hires"), settings.frame_buffer & fb_hwfbe ? 1 : 0l);
    ini->Write(_T("fb_get_info"), settings.frame_buffer & fb_get_info ? 1 : 0l);
    ini->Write(_T("fb_render"), settings.frame_buffer & fb_depth_render ? 1 : 0l);
    ini->Write(_T("detect_cpu_write"), settings.frame_buffer & fb_cpu_write_hack ? 1 : 0l);
    if (settings.frame_buffer & fb_read_back_to_screen)
      ini->Write(_T("read_back_to_screen"), 1);
    else if (settings.frame_buffer & fb_read_back_to_screen2)
      ini->Write(_T("read_back_to_screen"), 2);
    else
      ini->Write(_T("read_back_to_screen"), 0l);
  }

  wxFileOutputStream os(iniName);
  ((wxFileConfig*)ini)->Save(os);
}

GRTEXBUFFEREXT   grTextureBufferExt = NULL;
GRTEXBUFFEREXT   grTextureAuxBufferExt = NULL;
GRAUXBUFFEREXT   grAuxBufferExt = NULL;
GRSTIPPLE grStippleModeExt = NULL;
GRSTIPPLE grStipplePatternExt = NULL;
FxBool (FX_CALL *grKeyPressed)(FxU32) = NULL;

int GetTexAddrUMA(int tmu, int texsize)
{
  int addr = voodoo.tex_min_addr[0] + voodoo.tmem_ptr[0];
  voodoo.tmem_ptr[0] += texsize;
  voodoo.tmem_ptr[1] = voodoo.tmem_ptr[0];
  return addr;
}
int GetTexAddrNonUMA(int tmu, int texsize)
{
  int addr = voodoo.tex_min_addr[tmu] + voodoo.tmem_ptr[tmu];
  voodoo.tmem_ptr[tmu] += texsize;
  return addr;
}
GETTEXADDR GetTexAddr = GetTexAddrNonUMA;

// guLoadTextures - used to load the cursor and font textures
void guLoadTextures ()
{
  if (grTextureBufferExt)
  {
    int tbuf_size = 0;
    if (voodoo.max_tex_size <= 256)
    {
      grTextureBufferExt(  GR_TMU1, voodoo.tex_min_addr[GR_TMU1], GR_LOD_LOG2_256, GR_LOD_LOG2_256,
        GR_ASPECT_LOG2_1x1, GR_TEXFMT_RGB_565, GR_MIPMAPLEVELMASK_BOTH );
      tbuf_size = 8 * grTexCalcMemRequired(GR_LOD_LOG2_256, GR_LOD_LOG2_256,
        GR_ASPECT_LOG2_1x1, GR_TEXFMT_RGB_565);
    }
    else if (settings.scr_res_x <= 1024)
    {
      grTextureBufferExt(  GR_TMU0, voodoo.tex_min_addr[GR_TMU0], GR_LOD_LOG2_1024, GR_LOD_LOG2_1024,
        GR_ASPECT_LOG2_1x1, GR_TEXFMT_RGB_565, GR_MIPMAPLEVELMASK_BOTH );
      tbuf_size = grTexCalcMemRequired(GR_LOD_LOG2_1024, GR_LOD_LOG2_1024,
        GR_ASPECT_LOG2_1x1, GR_TEXFMT_RGB_565);
      grRenderBuffer( GR_BUFFER_TEXTUREBUFFER_EXT );
      grBufferClear (0, 0, 0xFFFF);
      grRenderBuffer( GR_BUFFER_BACKBUFFER );
    }
    else
    {
      grTextureBufferExt(  GR_TMU0, voodoo.tex_min_addr[GR_TMU0], GR_LOD_LOG2_2048, GR_LOD_LOG2_2048,
        GR_ASPECT_LOG2_1x1, GR_TEXFMT_RGB_565, GR_MIPMAPLEVELMASK_BOTH );
      tbuf_size = grTexCalcMemRequired(GR_LOD_LOG2_2048, GR_LOD_LOG2_2048,
        GR_ASPECT_LOG2_1x1, GR_TEXFMT_RGB_565);
      grRenderBuffer( GR_BUFFER_TEXTUREBUFFER_EXT );
      grBufferClear (0, 0, 0xFFFF);
      grRenderBuffer( GR_BUFFER_BACKBUFFER );
    }

    rdp.texbufs[0].tmu = GR_TMU0;
    rdp.texbufs[0].begin = voodoo.tex_min_addr[GR_TMU0];
    rdp.texbufs[0].end = rdp.texbufs[0].begin+tbuf_size;
    rdp.texbufs[0].count = 0;
    rdp.texbufs[0].clear_allowed = TRUE;
    offset_font = tbuf_size;
    if (voodoo.num_tmu > 1)
    {
      rdp.texbufs[1].tmu = GR_TMU1;
      rdp.texbufs[1].begin = voodoo.tex_UMA ? rdp.texbufs[0].end : voodoo.tex_min_addr[GR_TMU1];
      rdp.texbufs[1].end = rdp.texbufs[1].begin+tbuf_size;
      rdp.texbufs[1].count = 0;
      rdp.texbufs[1].clear_allowed = TRUE;
      if (voodoo.tex_UMA)
        offset_font += tbuf_size;
      else
        offset_texbuf1 = tbuf_size;
    }
  }
  else
    offset_font = 0;

#include "font.h"
  wxUint32 *data = (wxUint32*)font;
  wxUint32 cur;

  // ** Font texture **
  wxUint8 *tex8 = (wxUint8*)malloc(256*64);

  fontTex.smallLodLog2 = fontTex.largeLodLog2 = GR_LOD_LOG2_256;
  fontTex.aspectRatioLog2 = GR_ASPECT_LOG2_4x1;
  fontTex.format = GR_TEXFMT_ALPHA_8;
  fontTex.data = tex8;

  // Decompression: [1-bit inverse alpha --> 8-bit alpha]
  wxUint32 i,b;
  for (i=0; i<0x200; i++)
  {
    // cur = ~*(data++), byteswapped
#ifdef __VISUALC__
    cur = _byteswap_ulong(~*(data++));
#else
    cur = ~*(data++);
    cur = ((cur&0xFF)<<24)|(((cur>>8)&0xFF)<<16)|(((cur>>16)&0xFF)<<8)|((cur>>24)&0xFF);
#endif

    for (b=0x80000000; b!=0; b>>=1)
    {
      if (cur&b) *tex8 = 0xFF;
      else *tex8 = 0x00;
      tex8 ++;
    }
  }

  grTexDownloadMipMap (GR_TMU0,
    voodoo.tex_min_addr[GR_TMU0] + offset_font,
    GR_MIPMAPLEVELMASK_BOTH,
    &fontTex);

  offset_cursor = offset_font + grTexTextureMemRequired (GR_MIPMAPLEVELMASK_BOTH, &fontTex);

  free (fontTex.data);

  // ** Cursor texture **
#include "cursor.h"
  data = (wxUint32*)cursor;

  wxUint16 *tex16 = (wxUint16*)malloc(32*32*2);

  cursorTex.smallLodLog2 = cursorTex.largeLodLog2 = GR_LOD_LOG2_32;
  cursorTex.aspectRatioLog2 = GR_ASPECT_LOG2_1x1;
  cursorTex.format = GR_TEXFMT_ARGB_1555;
  cursorTex.data = tex16;

  // Conversion: [16-bit 1555 (swapped) --> 16-bit 1555]
  for (i=0; i<0x200; i++)
  {
    cur = *(data++);
    *(tex16++) = (wxUint16)(((cur&0x000000FF)<<8)|((cur&0x0000FF00)>>8));
    *(tex16++) = (wxUint16)(((cur&0x00FF0000)>>8)|((cur&0xFF000000)>>24));
  }

  grTexDownloadMipMap (GR_TMU0,
    voodoo.tex_min_addr[GR_TMU0] + offset_cursor,
    GR_MIPMAPLEVELMASK_BOTH,
    &cursorTex);

  // Round to higher 16
  offset_textures = ((offset_cursor + grTexTextureMemRequired (GR_MIPMAPLEVELMASK_BOTH, &cursorTex))
    & 0xFFFFFFF0) + 16;
  free (cursorTex.data);
}

#ifdef TEXTURE_FILTER
void DisplayLoadProgress(const wchar_t *format, ...)
{
  va_list args;
  wchar_t wbuf[INFO_BUF];
  char buf[INFO_BUF];

  // process input
  va_start(args, format);
  vswprintf(wbuf, INFO_BUF, format, args);
  va_end(args);

  // XXX: convert to multibyte
  wcstombs(buf, wbuf, INFO_BUF);

  if (fullscreen)
  {
    float x;
    set_message_combiner ();
    output (382, 380, 1, "LOADING TEXTURES. PLEASE WAIT...");
    int len = min (strlen(buf)*8, 1024);
    x = (1024-len)/2.0f;
    output (x, 360, 1, buf);
    grBufferSwap (0);
    grColorMask (FXTRUE, FXTRUE);
    grBufferClear (0, 0, 0xFFFF);
  }
}
#endif

int InitGfx (int evoodoo_using_window)
{
  if (fullscreen)
  {
    ReleaseGfx ();
  }

  OPEN_RDP_LOG ();  // doesn't matter if opens again; it will check for it
  OPEN_RDP_E_LOG ();
  LOG ("InitGfx ()\n");

  debugging = FALSE;
  rdp_reset ();

  char strConfigWrapperExt[] = "grConfigWrapperExt";
  GRCONFIGWRAPPEREXT grConfigWrapperExt = (GRCONFIGWRAPPEREXT)grGetProcAddress(strConfigWrapperExt);
  if (grConfigWrapperExt)
  {
    grConfigWrapperExt(settings.wrpResolution, settings.wrpVRAM * 1024 * 1024, settings.wrpFBO, settings.wrpAnisotropic);
  }

  // Initialize Glide
  grGlideInit ();

  // Select the Glide device
  grSstSelect (settings.card_id);

  // Is mirroring allowed?
  const char *extensions = grGetString (GR_EXTENSION);

  // Check which SST we are using and initialize stuff
  // Hiroshi Morii <koolsmoky@users.sourceforge.net>
  enum {
    GR_SSTTYPE_VOODOO  = 0,
    GR_SSTTYPE_SST96   = 1,
    GR_SSTTYPE_AT3D    = 2,
    GR_SSTTYPE_Voodoo2 = 3,
    GR_SSTTYPE_Banshee = 4,
    GR_SSTTYPE_Voodoo3 = 5,
    GR_SSTTYPE_Voodoo4 = 6,
    GR_SSTTYPE_Voodoo5 = 7
  };
  const char *hardware = grGetString(GR_HARDWARE);
  unsigned int SST_type = GR_SSTTYPE_VOODOO;
  if (strstr(hardware, "Rush")) {
    SST_type = GR_SSTTYPE_SST96;
  } else if (strstr(hardware, "Voodoo2")) {
    SST_type = GR_SSTTYPE_Voodoo2;
  } else if (strstr(hardware, "Voodoo Banshee")) {
    SST_type = GR_SSTTYPE_Banshee;
  } else if (strstr(hardware, "Voodoo3")) {
    SST_type = GR_SSTTYPE_Voodoo3;
  } else if (strstr(hardware, "Voodoo4")) {
    SST_type = GR_SSTTYPE_Voodoo4;
  } else if (strstr(hardware, "Voodoo5")) {
    SST_type = GR_SSTTYPE_Voodoo5;
  }
  // 2Mb Texture boundary
  voodoo.has_2mb_tex_boundary = (SST_type < GR_SSTTYPE_Banshee) && !evoodoo;
  // use UMA if available
  voodoo.tex_UMA = FALSE;
  //*
  if (strstr(extensions, " TEXUMA ")) {
    // we get better texture cache hits with UMA on
    grEnable(GR_TEXTURE_UMA_EXT);
    voodoo.tex_UMA = TRUE;
    LOG ("Using TEXUMA extension.\n");
  }
  //*/

  gfx_context = 0;
  wxUint32 res_data = settings.res_data;
  res_data |= ((evoodoo_using_window)?0x80000000:0x00000000);

  // punt if windowed mode is requested but the gfx
  // does not have windowed glide3x capabilities
  if (!evoodoo && evoodoo_using_window) {
    grGlideShutdown ();
    return FALSE;
  }

  // Select the window

  if (fb_hwfbe_enabled)
  {
    char strSstWinOpenExt[] ="grSstWinOpenExt";
    GRWINOPENEXT grSstWinOpenExt = (GRWINOPENEXT)grGetProcAddress(strSstWinOpenExt);
    if (grSstWinOpenExt)
      gfx_context = grSstWinOpenExt (wxPtrToUInt(gfx.hWnd),
      res_data,
      GR_REFRESH_60Hz,
      GR_COLORFORMAT_RGBA,
      GR_ORIGIN_UPPER_LEFT,
      fb_emulation_enabled?GR_PIXFMT_RGB_565:GR_PIXFMT_ARGB_8888, //32b color is not compatible with fb emulation
      2,    // Double-buffering
      1);   // 1 auxillary buffer
  }
  if (!gfx_context)
    gfx_context = grSstWinOpen (wxPtrToUInt(gfx.hWnd),
    res_data,
    GR_REFRESH_60Hz,
    GR_COLORFORMAT_RGBA,
    GR_ORIGIN_UPPER_LEFT,
    2,    // Double-buffering
    1);   // 1 auxillary buffer

  if (!gfx_context)
  {
    wxMessageBox(_T("Error setting display mode"), _T("Error"), wxOK|wxICON_EXCLAMATION);
    //    grSstWinClose (gfx_context);
    grGlideShutdown ();
    return FALSE;
  }

  // get the # of TMUs available
  grGet (GR_NUM_TMU, 4, (FxI32*)&voodoo.num_tmu);
  // get maximal texture size
  grGet (GR_MAX_TEXTURE_SIZE, 4, (FxI32*)&voodoo.max_tex_size);
  voodoo.sup_large_tex = (voodoo.max_tex_size > 256 && !(settings.hacks & hack_PPL));

  //num_tmu = 1;
  if (voodoo.tex_UMA)
  {
    GetTexAddr = GetTexAddrUMA;
    voodoo.tex_min_addr[0] = voodoo.tex_min_addr[1] = grTexMinAddress(GR_TMU0);
    voodoo.tex_max_addr[0] = voodoo.tex_max_addr[1] = grTexMaxAddress(GR_TMU0);
  }
  else
  {
    GetTexAddr = GetTexAddrNonUMA;
    voodoo.tex_min_addr[0] = grTexMinAddress(GR_TMU0);
    voodoo.tex_min_addr[1] = grTexMinAddress(GR_TMU1);
    voodoo.tex_max_addr[0] = grTexMaxAddress(GR_TMU0);
    voodoo.tex_max_addr[1] = grTexMaxAddress(GR_TMU1);
  }

  if (strstr (extensions, "TEXMIRROR") && !(settings.hacks&hack_Zelda)) //zelda's trees suffer from hardware mirroring
    voodoo.sup_mirroring = 1;
  else
    voodoo.sup_mirroring = 0;

  if (strstr (extensions, "TEXFMT"))  //VSA100 texture format extension
    voodoo.sup_32bit_tex = TRUE;
  else
    voodoo.sup_32bit_tex = FALSE;

  voodoo.gamma_correction = 0;
  if (strstr(extensions, "GETGAMMA"))
    grGet(GR_GAMMA_TABLE_ENTRIES, sizeof(voodoo.gamma_table_size), &voodoo.gamma_table_size);

  if (fb_hwfbe_enabled)
  {
    if (char * extstr = (char*)strstr(extensions, "TEXTUREBUFFER"))
    {
      if (!strncmp(extstr, "TEXTUREBUFFER", 13))
      {
        char strTextureBufferExt[] = "grTextureBufferExt";
        grTextureBufferExt = (GRTEXBUFFEREXT) grGetProcAddress(strTextureBufferExt);
        char strTextureAuxBufferExt[] = "grTextureAuxBufferExt";
        grTextureAuxBufferExt = (GRTEXBUFFEREXT) grGetProcAddress(strTextureAuxBufferExt);
        char strAuxBufferExt[] = "grAuxBufferExt";
        grAuxBufferExt = (GRAUXBUFFEREXT) grGetProcAddress(strAuxBufferExt);
      }
    }
    else
      settings.frame_buffer &= ~fb_hwfbe;
  }
  else
    grTextureBufferExt = 0;

#ifdef __WINDOWS__
  wxDynamicLibrary glidelib(_T("glide3x"));
  if (glidelib.IsLoaded())
  {
    if (glidelib.HasSymbol(_T("_grStippleMode@4")))
      grStippleModeExt = (GRSTIPPLE)glidelib.GetSymbol(_T("_grStippleMode@4"));
    if (glidelib.HasSymbol(_T("_grStipplePattern@4")))
      grStipplePatternExt = (GRSTIPPLE)glidelib.GetSymbol(_T("_grStipplePattern@4"));
  }
#else
  grStippleModeExt = (GRSTIPPLE)grStippleMode;
  grStipplePatternExt = (GRSTIPPLE)grStipplePattern;
#endif

  if (grStipplePatternExt)
    grStipplePatternExt(settings.stipple_pattern);

  char strKeyPressedExt[] = "grKeyPressedExt";
  grKeyPressed = (FxBool (FX_CALL *)(FxU32))grGetProcAddress (strKeyPressedExt);

  InitCombine();

#ifdef SIMULATE_VOODOO1
  voodoo.num_tmu = 1;
  voodoo.sup_mirroring = 0;
#endif

#ifdef SIMULATE_BANSHEE
  voodoo.num_tmu = 1;
  voodoo.sup_mirroring = 1;
#endif

  fullscreen = TRUE;

  if (evoodoo_using_window)
    ev_fullscreen = FALSE;
  else
    ev_fullscreen = TRUE;

  grCoordinateSpace (GR_WINDOW_COORDS);
  grVertexLayout (GR_PARAM_XY, offsetof(VERTEX,x), GR_PARAM_ENABLE);
  grVertexLayout (GR_PARAM_Q, offsetof(VERTEX,q), GR_PARAM_ENABLE);
  grVertexLayout (GR_PARAM_Z, offsetof(VERTEX,z), GR_PARAM_ENABLE);
  grVertexLayout (GR_PARAM_ST0, offsetof(VERTEX,coord[0]), GR_PARAM_ENABLE);
  grVertexLayout (GR_PARAM_ST1, offsetof(VERTEX,coord[2]), GR_PARAM_ENABLE);
  grVertexLayout (GR_PARAM_PARGB, offsetof(VERTEX,b), GR_PARAM_ENABLE);

  grCullMode(GR_CULL_NEGATIVE);

  if (settings.fog) //"FOGCOORD" extension
  {
    if (strstr (extensions, "FOGCOORD"))
    {
      GrFog_t fog_t[64];
      guFogGenerateLinear (fog_t, 0.0f, 255.0f);//(float)rdp.fog_multiplier + (float)rdp.fog_offset);//256.0f);

      for (int i = 63; i > 0; i--)
      {
        if (fog_t[i] - fog_t[i-1] > 63)
        {
          fog_t[i-1] = fog_t[i] - 63;
        }
      }
      fog_t[0] = 0;
      //      for (int f = 0; f < 64; f++)
      //      {
      //        FRDP("fog[%d]=%d->%f\n", f, fog_t[f], guFogTableIndexToW(f));
      //      }
      grFogTable (fog_t);
      grVertexLayout (GR_PARAM_FOG_EXT, offsetof(VERTEX,f), GR_PARAM_ENABLE);
    }
    else //not supported
      settings.fog = FALSE;
  }

  grDepthBufferMode (GR_DEPTHBUFFER_ZBUFFER);
  grDepthBufferFunction(GR_CMP_LESS);
  grDepthMask(FXTRUE);

  settings.res_x = settings.scr_res_x;
  settings.res_y = settings.scr_res_y;
  ChangeSize ();

  guLoadTextures ();
  ClearCache ();

  grCullMode (GR_CULL_DISABLE);
  grDepthBufferMode (GR_DEPTHBUFFER_ZBUFFER);
  grDepthBufferFunction (GR_CMP_ALWAYS);
  grRenderBuffer(GR_BUFFER_BACKBUFFER);
  grColorMask (FXTRUE, FXTRUE);
  grDepthMask (FXTRUE);
  grBufferClear (0, 0, 0xFFFF);
  grBufferSwap (0);
  grBufferClear (0, 0, 0xFFFF);
  grDepthMask (FXFALSE);
  grTexFilterMode (0, GR_TEXTUREFILTER_BILINEAR, GR_TEXTUREFILTER_BILINEAR);
  grTexFilterMode (1, GR_TEXTUREFILTER_BILINEAR, GR_TEXTUREFILTER_BILINEAR);
  grTexClampMode (0, GR_TEXTURECLAMP_CLAMP, GR_TEXTURECLAMP_CLAMP);
  grTexClampMode (1, GR_TEXTURECLAMP_CLAMP, GR_TEXTURECLAMP_CLAMP);
  grClipWindow (0, 0, settings.scr_res_x, settings.scr_res_y);
  rdp.update |= UPDATE_SCISSOR | UPDATE_COMBINE | UPDATE_ZBUF_ENABLED | UPDATE_CULL_MODE;

#ifdef TEXTURE_FILTER // Hiroshi Morii <koolsmoky@users.sourceforge.net>
  if (!settings.ghq_use)
  {
    settings.ghq_use = settings.ghq_fltr || settings.ghq_enht /*|| settings.ghq_cmpr*/ || settings.ghq_hirs;
    if (settings.ghq_use)
    {
      /* Plugin path */
      int options = texfltr[settings.ghq_fltr]|texenht[settings.ghq_enht]|texcmpr[settings.ghq_cmpr]|texhirs[settings.ghq_hirs];
      if (settings.ghq_enht_cmpr)
        options |= COMPRESS_TEX;
      if (settings.ghq_hirs_cmpr)
        options |= COMPRESS_HIRESTEX;
      //      if (settings.ghq_enht_tile)
      //        options |= TILE_TEX;
      if (settings.ghq_hirs_tile)
        options |= TILE_HIRESTEX;
      if (settings.ghq_enht_f16bpp)
        options |= FORCE16BPP_TEX;
      if (settings.ghq_hirs_f16bpp)
        options |= FORCE16BPP_HIRESTEX;
      if (settings.ghq_enht_gz)
        options |= GZ_TEXCACHE;
      if (settings.ghq_hirs_gz)
        options |= GZ_HIRESTEXCACHE;
      if (settings.ghq_cache_save)
        options |= (DUMP_TEXCACHE|DUMP_HIRESTEXCACHE);
      if (settings.ghq_hirs_let_texartists_fly)
        options |= LET_TEXARTISTS_FLY;
      if (settings.ghq_hirs_dump)
        options |= DUMP_TEX;

      ghq_dmptex_toggle_key = 0;

      settings.ghq_use = (int)ext_ghq_init(voodoo.max_tex_size, // max texture width supported by hardware
        voodoo.max_tex_size, // max texture height supported by hardware
        voodoo.sup_32bit_tex?32:16, // max texture bpp supported by hardware
        options,
        settings.ghq_cache_size * 1024*1024, // cache texture to system memory
        pluginPath.wchar_str(), // plugin path
        rdp.RomName.wchar_str(), // name of ROM. must be no longer than 256 characters
        DisplayLoadProgress);
    }
  }
  if (settings.ghq_use && strstr (extensions, "TEXMIRROR"))
    voodoo.sup_mirroring = 1;
#endif

  return TRUE;
}

void ReleaseGfx ()
{
  LOG("ReleaseGfx ()\n");

  // Restore gamma settings
  if (voodoo.gamma_correction)
  {
    if (voodoo.gamma_table_r)
      grLoadGammaTable(voodoo.gamma_table_size, voodoo.gamma_table_r, voodoo.gamma_table_g, voodoo.gamma_table_b);
    else
      guGammaCorrectionRGB(1.3f, 1.3f, 1.3f); //1.3f is default 3dfx gamma for everything but desktop
    voodoo.gamma_correction = 0;
  }

  // Release graphics
  grSstWinClose (gfx_context);

  // Shutdown glide
  grGlideShutdown();

  fullscreen = FALSE;
  rdp.window_changed = TRUE;
}

//
// DllMain - called when the DLL is loaded, use this to get the DLL's instance
//
wxMutex *mutexProcessDList = NULL;

class wxDLLApp : public wxApp
{
public:
  virtual bool OnInit();
};

IMPLEMENT_APP_NO_MAIN(wxDLLApp)

bool wxDLLApp::OnInit()
{
  if (mutexProcessDList == NULL)
    mutexProcessDList = new wxMutex(wxMUTEX_DEFAULT);
  wxImage::AddHandler(new wxPNGHandler);
  wxImage::AddHandler(new wxJPEGHandler);
  PluginPath();
  return true;
}

#ifndef __WINDOWS__
int __attribute__ ((constructor)) DllLoad(void);
int __attribute__ ((destructor)) DllUnload(void);
#endif

// Called when the library is loaded and before dlopen() returns
int DllLoad(void)
{
    int argc = 0;
    char **argv = NULL;
    wxEntryStart(argc, argv);
    if (wxTheApp)
      return wxTheApp->CallOnInit() ? TRUE : FALSE;
    return 0;
}

// Called when the library is unloaded and before dlclose() returns
int DllUnload(void)
{
    if ( wxTheApp )
      wxTheApp->OnExit();
    wxEntryCleanup();
    return TRUE;
}

#ifdef __WINDOWS__
extern "C" int WINAPI DllMain (HINSTANCE hinstDLL,
                     wxUint32 fdwReason,
                     LPVOID lpReserved)
{
  sprintf (out_buf, "DllMain (%08lx - %d)\n", hinstDLL, fdwReason);
  LOG (out_buf);

  if (fdwReason == DLL_PROCESS_ATTACH)
  {
    wxSetInstance(hinstDLL);
    return DllLoad();
  }
  else if (fdwReason == DLL_PROCESS_DETACH)
  {
    if (GFXWindow != NULL)
      GFXWindow->SetHWND(NULL);
    return DllUnload();
  }
  return TRUE;
}
#endif

void CALL ReadScreen(void **dest, int *width, int *height)
{
  *width = settings.res_x;
  *height = settings.res_y;
  wxUint8 * buff = (wxUint8*)malloc(settings.res_x * settings.res_y * 3);
  wxUint8 * line = buff;
  *dest = (void*)buff;

  if (!fullscreen)
  {
    for (wxUint32 y=0; y<settings.res_y; y++)
    {
      for (wxUint32 x=0; x<settings.res_x; x++)
      {
        line[x*3] = 0x20;
        line[x*3+1] = 0x7f;
        line[x*3+2] = 0x40;
      }
    }
    LOG ("ReadScreen. not in the fullscreen!\n");
    return;
  }

  GrLfbInfo_t info;
  info.size = sizeof(GrLfbInfo_t);
  if (grLfbLock (GR_LFB_READ_ONLY,
    GR_BUFFER_FRONTBUFFER,
    GR_LFBWRITEMODE_565,
    GR_ORIGIN_UPPER_LEFT,
    FXFALSE,
    &info))
  {
    wxUint32 offset_src=info.strideInBytes*(settings.scr_res_y-1);

    // Copy the screen
    wxUint8 r, g, b;
    if (info.writeMode == GR_LFBWRITEMODE_8888)
    {
      wxUint32 col;
      for (wxUint32 y=0; y<settings.res_y; y++)
      {
        wxUint32 *ptr = (wxUint32*)((wxUint8*)info.lfbPtr + offset_src);
        for (wxUint32 x=0; x<settings.res_x; x++)
        {
          col = *(ptr++);
          r = (wxUint8)((col >> 16) & 0xFF);
          g = (wxUint8)((col >> 8) & 0xFF);
          b = (wxUint8)(col & 0xFF);
          line[x*3] = b;
          line[x*3+1] = g;
          line[x*3+2] = r;
        }
        line += settings.res_x * 3;
        offset_src -= info.strideInBytes;
      }
    }
    else
    {
      wxUint16 col;
      for (wxUint32 y=0; y<settings.res_y; y++)
      {
        wxUint16 *ptr = (wxUint16*)((wxUint8*)info.lfbPtr + offset_src);
        for (wxUint32 x=0; x<settings.res_x; x++)
        {
          col = *(ptr++);
          r = (wxUint8)((float)(col >> 11) / 31.0f * 255.0f);
          g = (wxUint8)((float)((col >> 5) & 0x3F) / 63.0f * 255.0f);
          b = (wxUint8)((float)(col & 0x1F) / 31.0f * 255.0f);
          line[x*3] = b;
          line[x*3+1] = g;
          line[x*3+2] = r;
        }
        line += settings.res_x * 3;
        offset_src -= info.strideInBytes;
      }
    }
    // Unlock the frontbuffer
    grLfbUnlock (GR_LFB_READ_ONLY, GR_BUFFER_FRONTBUFFER);
  }
  LOG ("ReadScreen. Success.\n");
}

/******************************************************************
Function: CaptureScreen
Purpose:  This function dumps the current frame to a file
input:    pointer to the directory to save the file to
output:   none
*******************************************************************/
EXPORT void CALL CaptureScreen ( char * Directory )
{
  capture_screen = 1;
  capture_path = wxString::FromAscii(Directory);
}

/******************************************************************
Function: ChangeWindow
Purpose:  to change the window between fullscreen and window
mode. If the window was in fullscreen this should
change the screen to window mode and vice vesa.
input:    none
output:   none
*******************************************************************/
EXPORT void CALL ChangeWindow (void)
{
  LOG ("ChangeWindow()\n");

  if (evoodoo)
  {
    char strWrapperFullScreenResolutionExt[] = "grWrapperFullScreenResolutionExt";
    if (!ev_fullscreen)
    {
      to_fullscreen = TRUE;
#ifdef __WINDOWS__
      if (gfx.hStatusBar)
        ShowWindow( gfx.hStatusBar, SW_HIDE );
      ShowCursor( FALSE );
#endif
      GRWRAPPERFULLSCREENRESOLUTIONEXT grWrapperFullScreenResolutionExt =
        (GRWRAPPERFULLSCREENRESOLUTIONEXT)grGetProcAddress(strWrapperFullScreenResolutionExt);
      if (grWrapperFullScreenResolutionExt) {
        wxUint32 _width, _height = 0;
        settings.res_data_org = settings.res_data;
        settings.res_data = grWrapperFullScreenResolutionExt(&_width, &_height);
        settings.scr_res_x = settings.res_x = _width;
        settings.scr_res_y = settings.res_y = _height;
      }
    }
    else
    {
      ReleaseGfx ();
      GRWRAPPERFULLSCREENRESOLUTIONEXT grWrapperFullScreenResolutionExt =
        (GRWRAPPERFULLSCREENRESOLUTIONEXT)grGetProcAddress(strWrapperFullScreenResolutionExt);
      if (grWrapperFullScreenResolutionExt != NULL)
      {
        settings.res_data = settings.res_data_org;
        settings.scr_res_x = settings.res_x = resolutions[settings.res_data][0];
        settings.scr_res_y = settings.res_y = resolutions[settings.res_data][1];
      }
      InitGfx (TRUE);
#ifdef __WINDOWS__
      ShowCursor( TRUE );
      if (gfx.hStatusBar)
        ShowWindow( gfx.hStatusBar, SW_SHOW );
      SetWindowLong (gfx.hWnd, GWL_WNDPROC, (long)oldWndProc);
#endif
    }
  }
  else
  {
    // Go to fullscreen at next dlist
    // This is for compatibility with 1964, which reloads the plugin
    //  when switching to fullscreen
    if (!fullscreen)
    {
      to_fullscreen = TRUE;
#ifdef __WINDOWS__
      if (gfx.hStatusBar)
        ShowWindow( gfx.hStatusBar, SW_HIDE );
      ShowCursor( FALSE );
#endif
    }
    else
    {
      ReleaseGfx ();
#ifdef __WINDOWS__
      ShowCursor( TRUE );
      if (gfx.hStatusBar)
        ShowWindow( gfx.hStatusBar, SW_SHOW );
      // SetWindowLong fixes the following Windows XP Banshee issues:
      // 1964 crash error when loading another rom.
      // All N64 emu's minimize, restore crashes.
      SetWindowLong (gfx.hWnd, GWL_WNDPROC, (long)oldWndProc);
#endif
    }
  }
}

/******************************************************************
Function: CloseDLL
Purpose:  This function is called when the emulator is closing
down allowing the dll to de-initialise.
input:    none
output:   none
*******************************************************************/
void CALL CloseDLL (void)
{
  LOG ("CloseDLL ()\n");

  // re-set the old window proc
#ifdef WINPROC_OVERRIDE
  SetWindowLong (gfx.hWnd, GWL_WNDPROC, (long)oldWndProc);
#endif

#ifdef ALTTAB_FIX
  if (hhkLowLevelKybd)
  {
    UnhookWindowsHookEx(hhkLowLevelKybd);
    hhkLowLevelKybd = 0;
  }
#endif

  //CLOSELOG ();

#ifdef TEXTURE_FILTER // Hiroshi Morii <koolsmoky@users.sourceforge.net>
  if (settings.ghq_use)
  {
    ext_ghq_shutdown();
    settings.ghq_use = 0;
  }
#endif
  if (fullscreen)
    ReleaseGfx ();
  ZLUT_release();
  ClearCache ();
  delete[] voodoo.gamma_table_r;
  voodoo.gamma_table_r = 0;
  delete[] voodoo.gamma_table_g;
  voodoo.gamma_table_g = 0;
  delete[] voodoo.gamma_table_b;
  voodoo.gamma_table_b = 0;
}

/******************************************************************
Function: DllTest
Purpose:  This function is optional function that is provided
to allow the user to test the dll
input:    a handle to the window that calls this function
output:   none
*******************************************************************/
void CALL DllTest ( HWND hParent )
{
}

/******************************************************************
Function: DrawScreen
Purpose:  This function is called when the emulator receives a
WM_PAINT message. This allows the gfx to fit in when
it is being used in the desktop.
input:    none
output:   none
*******************************************************************/
void CALL DrawScreen (void)
{
  LOG ("DrawScreen ()\n");
}

/******************************************************************
Function: GetDllInfo
Purpose:  This function allows the emulator to gather information
about the dll by filling in the PluginInfo structure.
input:    a pointer to a PLUGIN_INFO stucture that needs to be
filled by the function. (see def above)
output:   none
*******************************************************************/
void CALL GetDllInfo ( PLUGIN_INFO * PluginInfo )
{
  LOG ("GetDllInfo ()\n");
  PluginInfo->Version = 0x0103;     // Set to 0x0103
  PluginInfo->Type  = PLUGIN_TYPE_GFX;  // Set to PLUGIN_TYPE_GFX
  sprintf (PluginInfo->Name, "Glide64 "G64_VERSION RELTIME);  // Name of the DLL

  // If DLL supports memory these memory options then set them to TRUE or FALSE
  //  if it does not support it
  PluginInfo->NormalMemory = TRUE;  // a normal wxUint8 array
  PluginInfo->MemoryBswaped = TRUE; // a normal wxUint8 array where the memory has been pre
  // bswap on a dword (32 bits) boundry
}

/******************************************************************
Function: InitiateGFX
Purpose:  This function is called when the DLL is started to give
information from the emulator that the n64 graphics
uses. This is not called from the emulation thread.
Input:    Gfx_Info is passed to this function which is defined
above.
Output:   TRUE on success
FALSE on failure to initialise

** note on interrupts **:
To generate an interrupt set the appropriate bit in MI_INTR_REG
and then call the function CheckInterrupts to tell the emulator
that there is a waiting interrupt.
*******************************************************************/

int CALL InitiateGFX (GFX_INFO Gfx_Info)
{
  LOG ("InitiateGFX (*)\n");
  voodoo.num_tmu = 2;

  // Assume scale of 1 for debug purposes
  rdp.scale_x = 1.0f;
  rdp.scale_y = 1.0f;

  memset (&settings, 0, sizeof(SETTINGS));
  ReadSettings ();
  char name[21] = "DEFAULT";
  ReadSpecialSettings (name);

#ifdef FPS
  fps_last = wxDateTime::UNow();
#endif

  debug_init ();    // Initialize debugger

  gfx = Gfx_Info;
#ifdef __WINDOWS__
  if (GFXWindow == NULL)
    GFXWindow = new wxWindow();
  GFXWindow->SetHWND(gfx.hWnd);
#endif

#ifdef WINPROC_OVERRIDE
  // [H.Morii] inject our own winproc so that "alt-enter to fullscreen"
  // message is shown when the emulator window is activated.
  WNDPROC curWndProc = (WNDPROC)GetWindowLong(gfx.hWnd, GWL_WNDPROC);
  if (curWndProc && curWndProc != (WNDPROC)WndProc) {
    oldWndProc = (WNDPROC)SetWindowLong (gfx.hWnd, GWL_WNDPROC, (long)WndProc);
  }
#endif

  util_init ();
  math_init ();
  TexCacheInit ();
  CRC_BuildTable();
  CountCombine();
  if (fb_depth_render_enabled)
    ZLUT_init();
  voodoo.has_2mb_tex_boundary = 1;

  return TRUE;
}

/******************************************************************
Function: MoveScreen
Purpose:  This function is called in response to the emulator
receiving a WM_MOVE passing the xpos and ypos passed
from that message.
input:    xpos - the x-coordinate of the upper-left corner of the
client area of the window.
ypos - y-coordinate of the upper-left corner of the
client area of the window.
output:   none
*******************************************************************/
void CALL MoveScreen (int xpos, int ypos)
{
  LOG ("MoveScreen (" << xpos << ", " << ypos << ")\n");
  rdp.window_changed = TRUE;
}

/******************************************************************
Function: RomClosed
Purpose:  This function is called when a rom is closed.
input:    none
output:   none
*******************************************************************/
void CALL RomClosed (void)
{
  LOG ("RomClosed ()\n");

  CLOSE_RDP_LOG ();
  CLOSE_RDP_E_LOG ();
  rdp.window_changed = TRUE;
  romopen = FALSE;
  if (fullscreen && evoodoo)
    ReleaseGfx ();
}

static void CheckDRAMSize()
{
  wxUint32 test;
  GLIDE64_TRY
  {
    test = gfx.RDRAM[0x007FFFFF] + 1;
  }
  GLIDE64_CATCH
  {
    test = 0;
  }
  if (test)
    BMASK = 0x7FFFFF;
  else
    BMASK = WMASK;
#ifdef LOGGING
  sprintf (out_buf, "Detected RDRAM size: %08lx\n", BMASK);
  LOG (out_buf);
#endif
}

/******************************************************************
Function: RomOpen
Purpose:  This function is called when a rom is open. (from the
emulation thread)
input:    none
output:   none
*******************************************************************/
void CALL RomOpen (void)
{
  if (fullscreen && evoodoo)
    ReleaseGfx ();
  LOG ("RomOpen ()\n");
  no_dlist = TRUE;
  romopen = TRUE;
  ucode_error_report = TRUE;	// allowed to report ucode errors
  rdp_reset ();

  // Get the country code & translate to NTSC(0) or PAL(1)
  wxUint16 code = ((wxUint16*)gfx.HEADER)[0x1F^1];

  if (code == 0x4400) region = 1; // Germany (PAL)
  if (code == 0x4500) region = 0; // USA (NTSC)
  if (code == 0x4A00) region = 0; // Japan (NTSC)
  if (code == 0x5000) region = 1; // Europe (PAL)
  if (code == 0x5500) region = 0; // Australia (NTSC)

  char name[21] = "DEFAULT";
  ReadSpecialSettings (name);

  // get the name of the ROM
  for (int i=0; i<20; i++)
    name[i] = gfx.HEADER[(32+i)^3];
  name[20] = 0;

  // remove all trailing spaces
  while (name[strlen(name)-1] == ' ')
    name[strlen(name)-1] = 0;

  rdp.RomName = wxString::FromAscii(name);
  ReadSpecialSettings (name);
  ClearCache ();

  CheckDRAMSize();

  OPEN_RDP_LOG ();
  OPEN_RDP_E_LOG ();


  // ** EVOODOO EXTENSIONS **
  if (!fullscreen)
  {
    grGlideInit ();
    grSstSelect (0);
  }
  const char *extensions = grGetString (GR_EXTENSION);
  if (!fullscreen)
  {
    grGlideShutdown ();

    if (strstr (extensions, "EVOODOO"))
      evoodoo = 1;
    else
      evoodoo = 0;

    if (evoodoo)
      InitGfx (!ev_fullscreen);
  }

  if (strstr (extensions, "ROMNAME"))
  {
    char strSetRomName[] = "grSetRomName";
    void (FX_CALL *grSetRomName)(char*);
    grSetRomName = (void (FX_CALL *)(char*))grGetProcAddress (strSetRomName);
    grSetRomName (name);
  }
  // **
}

/******************************************************************
Function: ShowCFB
Purpose:  Useally once Dlists are started being displayed, cfb is
ignored. This function tells the dll to start displaying
them again.
input:    none
output:   none
*******************************************************************/
int no_dlist = TRUE;
void CALL ShowCFB (void)
{
  no_dlist = TRUE;
  LOG ("ShowCFB ()\n");
}

void drawViRegBG()
{
  LRDP("drawViRegBG\n");
  wxUint32 VIwidth = *gfx.VI_WIDTH_REG;
  FB_TO_SCREEN_INFO fb_info;
  fb_info.width  = VIwidth;
  fb_info.height = (wxUint32)rdp.vi_height;
  if (fb_info.height == 0)
  {
    LRDP("Image height = 0 - skipping\n");
    return;
  }
  fb_info.ul_x = 0;

  fb_info.lr_x = VIwidth - 1;
  //  fb_info.lr_x = (wxUint32)rdp.vi_width - 1;
  fb_info.ul_y = 0;
  fb_info.lr_y = fb_info.height - 1;
  fb_info.opaque = 1;
  if (settings.hacks&hack_RE2)
  {
    fb_info.addr   = (*gfx.VI_ORIGIN_REG) - (VIwidth<<2);
    fb_info.size   = 3;
  }
  else if (rdp.ci_size > 1)
  {
    fb_info.addr   = (*gfx.VI_ORIGIN_REG) - (VIwidth<<(rdp.ci_size-1));
    fb_info.size   = rdp.ci_size;
  }
  else
  {
    fb_info.addr   = (*gfx.VI_ORIGIN_REG) - (VIwidth<<1);
    fb_info.size   = 2;
  }
  rdp.last_bg = fb_info.addr;

  bool drawn = DrawFrameBufferToScreen(fb_info);
  if (settings.hacks&hack_Lego && drawn)
  {
    rdp.updatescreen = 1;
    newSwapBuffers ();
    DrawFrameBufferToScreen(fb_info);
  }
}

void drawNoFullscreenMessage();

static void DrawFrameBuffer ()
{
  if (!fullscreen)
  {
    drawNoFullscreenMessage();
  }
  if (to_fullscreen)
  {
    to_fullscreen = FALSE;

    if (!InitGfx (FALSE))
    {
      LOG ("FAILED!!!\n");
      return;
    }
    fullscreen = TRUE;
  }

  if (fullscreen)
  {
    grDepthMask (FXTRUE);
    grColorMask (FXTRUE, FXTRUE);
    grBufferClear (0, 0, 0xFFFF);
    drawViRegBG();
  }
}

/******************************************************************
Function: UpdateScreen
Purpose:  This function is called in response to a vsync of the
screen were the VI bit in MI_INTR_REG has already been
set
input:    none
output:   none
*******************************************************************/
wxUint32 update_screen_count = 0;
void CALL UpdateScreen (void)
{
#ifdef LOG_KEY
  if (CheckKeyPressed(G64_VK_SPACE, 0x0001))
  {
    LOG ("KEY!!!\n");
  }
#endif
  char out_buf[512];
  sprintf (out_buf, "UpdateScreen (). distance: %d\n", (int)(*gfx.VI_ORIGIN_REG) - (int)((*gfx.VI_WIDTH_REG) << 2));
  LOG (out_buf);
  //  LOG ("UpdateScreen ()\n");

  wxUint32 width = (*gfx.VI_WIDTH_REG) << 1;
  if (fullscreen && (*gfx.VI_ORIGIN_REG  > width))
    update_screen_count++;

  // vertical interrupt has occured, increment counter
  vi_count ++;

#ifdef FPS
  // Check frames per second
  fps_next = wxDateTime::UNow();
  wxTimeSpan difference = fps_next - fps_last;
  double diff_secs = difference.GetMilliseconds().ToDouble() / 1000.0;
  if (diff_secs > 0.5f)
  {
    fps = (float)(fps_count / diff_secs);
    vi = (float)(vi_count / diff_secs);
    ntsc_percent = vi / 0.6f;
    pal_percent = vi / 0.5f;
    fps_last = fps_next;
    fps_count = 0;
    vi_count = 0;
  }
#endif
  //*
  wxUint32 limit = (settings.hacks&hack_Lego) ? 15 : 30;
  if ((settings.frame_buffer&fb_cpu_write_hack) && (update_screen_count > limit) && (rdp.last_bg == 0))
  {
    LRDP("DirectCPUWrite hack!\n");
    update_screen_count = 0;
    no_dlist = TRUE;
    ClearCache ();
    UpdateScreen();
    return;
  }
  //*/
  //*
  if( no_dlist )
  {
    if( *gfx.VI_ORIGIN_REG  > width )
    {
      ChangeSize ();
      LRDP("ChangeSize done\n");
      DrawFrameBuffer();
      LRDP("DrawFrameBuffer done\n");
      rdp.updatescreen = 1;
      newSwapBuffers ();
    }
    return;
  }
  //*/
  if (settings.swapmode == 0)
    newSwapBuffers ();
}

static void DrawWholeFrameBufferToScreen()
{
  static wxUint32 toScreenCI = 0;
  if (rdp.ci_width < 200)
    return;
  if (rdp.cimg == toScreenCI)
    return;
  toScreenCI = rdp.cimg;
  FB_TO_SCREEN_INFO fb_info;
  fb_info.addr   = rdp.cimg;
  fb_info.size   = rdp.ci_size;
  fb_info.width  = rdp.ci_width;
  fb_info.height = rdp.ci_height;
  if (fb_info.height == 0)
    return;
  fb_info.ul_x = 0;
  fb_info.lr_x = rdp.ci_width-1;
  fb_info.ul_y = 0;
  fb_info.lr_y = rdp.ci_height-1;
  fb_info.opaque = 0;
  DrawFrameBufferToScreen(fb_info);
  if (!(settings.frame_buffer & fb_ref))
    memset(gfx.RDRAM+rdp.cimg, 0, (rdp.ci_width*rdp.ci_height)<<rdp.ci_size>>1);
}

static void GetGammaTable()
{
  char strGetGammaTableExt[] = "grGetGammaTableExt";
  void (FX_CALL *grGetGammaTableExt)(FxU32, FxU32*, FxU32*, FxU32*) =
    (void (FX_CALL *)(FxU32, FxU32*, FxU32*, FxU32*))grGetProcAddress(strGetGammaTableExt);
  if (grGetGammaTableExt)
  {
    voodoo.gamma_table_r = new FxU32[voodoo.gamma_table_size];
    voodoo.gamma_table_g = new FxU32[voodoo.gamma_table_size];
    voodoo.gamma_table_b = new FxU32[voodoo.gamma_table_size];
    grGetGammaTableExt(voodoo.gamma_table_size, voodoo.gamma_table_r, voodoo.gamma_table_g, voodoo.gamma_table_b);
  }
}

wxUint32 curframe = 0;
void newSwapBuffers()
{
  if (rdp.updatescreen)
  {
    rdp.updatescreen = 0;

    LRDP("swapped\n");

    // Allow access to the whole screen
    if (fullscreen)
    {
      rdp.update |= UPDATE_SCISSOR | UPDATE_COMBINE | UPDATE_ZBUF_ENABLED | UPDATE_CULL_MODE;
      grClipWindow (0, 0, settings.scr_res_x, settings.scr_res_y);
      grDepthBufferFunction (GR_CMP_ALWAYS);
      grDepthMask (FXFALSE);
      grCullMode (GR_CULL_DISABLE);

      if ((settings.show_fps & 0xF) || settings.clock)
        set_message_combiner ();
#ifdef FPS
      float y = 0;//(float)settings.res_y;
      if (settings.show_fps & 0x0F)
      {
        if (settings.show_fps & 4)
        {
          if (region)   // PAL
            output (0, y, 1, "%d%% ", (int)pal_percent);
          else
            output (0, y, 1, "%d%% ", (int)ntsc_percent);
          y += 16;
        }
        if (settings.show_fps & 2)
        {
          output (0, y, 1, "VI/s: %.02f ", vi);
          y += 16;
        }
        if (settings.show_fps & 1)
          output (0, y, 1, "FPS: %.02f ", fps);
      }
#endif

      if (settings.clock)
      {
        if (settings.clock_24_hr)
        {
          output (956.0f, 0, 1, (char*)wxDateTime::Now().Format(wxT("%H:%M:%S")).char_str(), 0);
        }
        else
        {
          output (930.0f, 0, 1, (char*)wxDateTime::Now().Format(wxT("%I:%M:%S %p")).char_str(), 0);
        }
      }
      //hotkeys
      if (CheckKeyPressed(G64_VK_BACK, 0x0001))
      {
        hotkey_info.hk_filtering = 100;
        if (settings.filtering < 2)
          settings.filtering++;
        else
          settings.filtering = 0;
      }
      if ((abs((int)(frame_count - curframe)) > 3 ) && CheckKeyPressed(G64_VK_ALT, 0x8000))  //alt +
      {
        if (CheckKeyPressed(G64_VK_B, 0x8000))  //b
        {
          hotkey_info.hk_motionblur = 100;
          hotkey_info.hk_ref = 0;
          curframe = frame_count;
          settings.frame_buffer ^= fb_motionblur;
        }
        else if (CheckKeyPressed(G64_VK_V, 0x8000))  //v
        {
          hotkey_info.hk_ref = 100;
          hotkey_info.hk_motionblur = 0;
          curframe = frame_count;
          settings.frame_buffer ^= fb_ref;
        }
      }
      if (settings.buff_clear && (hotkey_info.hk_ref || hotkey_info.hk_motionblur || hotkey_info.hk_filtering))
      {
        set_message_combiner ();
        char buf[256];
        buf[0] = 0;
        char * message = 0;
        if (hotkey_info.hk_ref)
        {
          if (settings.frame_buffer & fb_ref)
            message = strcat(buf, "FB READ ALWAYS: ON");
          else
            message = strcat(buf, "FB READ ALWAYS: OFF");
          hotkey_info.hk_ref--;
        }
        if (hotkey_info.hk_motionblur)
        {
          if (settings.frame_buffer & fb_motionblur)
            message = strcat(buf, "  MOTION BLUR: ON");
          else
            message = strcat(buf, "  MOTION BLUR: OFF");
          hotkey_info.hk_motionblur--;
        }
        if (hotkey_info.hk_filtering)
        {
          switch (settings.filtering)
          {
          case 0:
            message = strcat(buf, "  FILTERING MODE: AUTOMATIC");
            break;
          case 1:
            message = strcat(buf, "  FILTERING MODE: FORCE BILINEAR");
            break;
          case 2:
            message = strcat(buf, "  FILTERING MODE: FORCE POINT-SAMPLED");
            break;
          }
          hotkey_info.hk_filtering--;
        }
        output (120.0f, 0.0f, 1, message, 0);
      }
    }

    if (capture_screen)
    {
      //char path[256];
      // Make the directory if it doesn't exist
      if (!wxDirExists(capture_path))
        wxMkdir(capture_path);
      wxString path;
      wxString romName = rdp.RomName;
      romName.Replace(wxT(" "), wxT("_"), true);
      romName.Replace(wxT(":"), wxT(";"), true);

      for (int i=1; ; i++)
      {
        path = capture_path;
        path += wxT("Glide64_");
        path += romName;
        path += wxT("_");
        if (i < 10)
          path += wxT("0");
        path << i << wxT(".") << ScreenShotFormats[settings.ssformat].extension;
        if (!wxFileName::FileExists(path))
          break;
      }

      wxUint32 offset_x = (wxUint32)rdp.offset_x;
      wxUint32 offset_y = (wxUint32)rdp.offset_y;
      wxUint32 image_width = settings.res_x - offset_x;
      wxUint32 image_height = settings.res_y - offset_y;

      GrLfbInfo_t info;
      info.size = sizeof(GrLfbInfo_t);
      if (grLfbLock (GR_LFB_READ_ONLY,
        GR_BUFFER_BACKBUFFER,
        GR_LFBWRITEMODE_565,
        GR_ORIGIN_UPPER_LEFT,
        FXFALSE,
        &info))
      {
        wxUint8 *ssimg = (wxUint8*)malloc(image_width * image_height * 3);
        int sspos = 0;
        wxUint32 offset_src=0;

        // Copy the screen
        if (info.writeMode == GR_LFBWRITEMODE_8888)
        {
          wxUint32 col;
          for (wxUint32 y=offset_y; y<settings.res_y; y++)
          {
            wxUint32 *ptr = (wxUint32*)((wxUint8*)info.lfbPtr + offset_src);
            ptr+=offset_x;
            for (wxUint32 x=offset_x; x<settings.res_x; x++)
            {
              col = *(ptr++);
              ssimg[sspos++] = (wxUint8)((col >> 16) & 0xFF);
              ssimg[sspos++] = (wxUint8)((col >> 8) & 0xFF);
              ssimg[sspos++] = (wxUint8)(col & 0xFF);
            }
            offset_src += info.strideInBytes;
          }
        }
        else
        {
          wxUint16 col;
          for (wxUint32 y=offset_y; y<settings.res_y; y++)
          {
            wxUint16 *ptr = (wxUint16*)((wxUint8*)info.lfbPtr + offset_src);
            ptr+=offset_x;
            for (wxUint32 x=offset_x; x<settings.res_x; x++)
            {
              col = *(ptr++);
              ssimg[sspos++] = (wxUint8)((float)(col >> 11) / 31.0f * 255.0f);
              ssimg[sspos++] = (wxUint8)((float)((col >> 5) & 0x3F) / 63.0f * 255.0f);
              ssimg[sspos++] = (wxUint8)((float)(col & 0x1F) / 31.0f * 255.0f);
            }
            offset_src += info.strideInBytes;
          }
        }
        // Unlock the backbuffer
        grLfbUnlock (GR_LFB_READ_ONLY, GR_BUFFER_BACKBUFFER);
        wxImage screenshot(image_width, image_height, ssimg);
        screenshot.SaveFile(path, ScreenShotFormats[settings.ssformat].type);
        capture_screen = 0;
      }
    }

    // Capture the screen if debug capture is set
    if (_debugger.capture)
    {
      // Allocate the screen
      _debugger.screen = new wxUint8 [(settings.res_x*settings.res_y) << 1];

      // Lock the backbuffer (already rendered)
      GrLfbInfo_t info;
      info.size = sizeof(GrLfbInfo_t);
      while (!grLfbLock (GR_LFB_READ_ONLY,
        GR_BUFFER_BACKBUFFER,
        GR_LFBWRITEMODE_565,
        GR_ORIGIN_UPPER_LEFT,
        FXFALSE,
        &info));

      wxUint32 offset_src=0, offset_dst=0;

      // Copy the screen
      for (wxUint32 y=0; y<settings.res_y; y++)
      {
        if (info.writeMode == GR_LFBWRITEMODE_8888)
        {
          wxUint32 *src = (wxUint32*)((wxUint8*)info.lfbPtr + offset_src);
          wxUint16 *dst = (wxUint16*)(_debugger.screen + offset_dst);
          wxUint8 r, g, b;
          wxUint32 col;
          for (unsigned int x = 0; x < settings.res_x; x++)
          {
            col = src[x];
            r = (wxUint8)((col >> 19) & 0x1F);
            g = (wxUint8)((col >> 10) & 0x3F);
            b = (wxUint8)((col >> 3)  & 0x1F);
            dst[x] = (r<<11)|(g<<5)|b;
          }
        }
        else
        {
          memcpy (_debugger.screen + offset_dst, (wxUint8*)info.lfbPtr + offset_src, settings.res_x << 1);
        }
        offset_dst += settings.res_x << 1;
        offset_src += info.strideInBytes;
      }

      // Unlock the backbuffer
      grLfbUnlock (GR_LFB_READ_ONLY, GR_BUFFER_BACKBUFFER);
    }

    if (fullscreen && debugging)
    {
      debug_keys ();
      debug_cacheviewer ();
      debug_mouse ();
    }

    if (settings.frame_buffer & fb_read_back_to_screen)
      DrawWholeFrameBufferToScreen();

    if (fullscreen)
    {
      if (fb_hwfbe_enabled && !(settings.hacks&hack_RE2) && !evoodoo)
        grAuxBufferExt( GR_BUFFER_AUXBUFFER );
      LOG ("BUFFER SWAPPED\n");
      grBufferSwap (settings.vsync);
      fps_count ++;
      if (*gfx.VI_STATUS_REG&0x08) //gamma correction is used
      {
        if (!voodoo.gamma_correction)
        {
          if (voodoo.gamma_table_size && !voodoo.gamma_table_r)
            GetGammaTable(); //save initial gamma tables
          guGammaCorrectionRGB(2.0f, 2.0f, 2.0f); //with gamma=2.0 gamma table is the same, as in N64
          voodoo.gamma_correction = 1;
        }
      }
      else
      {
        if (voodoo.gamma_correction)
        {
          if (voodoo.gamma_table_r)
            grLoadGammaTable(voodoo.gamma_table_size, voodoo.gamma_table_r, voodoo.gamma_table_g, voodoo.gamma_table_b);
          else
            guGammaCorrectionRGB(1.3f, 1.3f, 1.3f); //1.3f is default 3dfx gamma for everything but desktop
          voodoo.gamma_correction = 0;
        }
      }
    }

    if (_debugger.capture)
      debug_capture ();

    if (fullscreen)
    {
      if  (debugging || settings.wireframe || settings.buff_clear)
      {
        if (settings.hacks&hack_RE2 && fb_depth_render_enabled)
          grDepthMask (FXFALSE);
        else
          grDepthMask (FXTRUE);
        grBufferClear (0, 0, 0xFFFF);
      }
      /* //let the game to clear the buffers
      else
      {
      grDepthMask (FXTRUE);
      grColorMask (FXFALSE, FXFALSE);
      grBufferClear (0, 0, 0xFFFF);
      grColorMask (FXTRUE, FXTRUE);
      }
      */
    }

    if (settings.frame_buffer & fb_read_back_to_screen2)
      DrawWholeFrameBufferToScreen();

    frame_count ++;

    // Open/close debugger?
    if (CheckKeyPressed(G64_VK_SCROLL, 0x0001))
    {
      if (!debugging)
      {
        //if (settings.scr_res_x == 1024 && settings.scr_res_y == 768)
        {
          debugging = 1;

          // Recalculate screen size, don't resize screen
          settings.res_x = (wxUint32)(settings.scr_res_x * 0.625f);
          settings.res_y = (wxUint32)(settings.scr_res_y * 0.625f);

          ChangeSize ();
        }
      } else {
        debugging = 0;

        settings.res_x = settings.scr_res_x;
        settings.res_y = settings.scr_res_y;

        ChangeSize ();
      }
    }

    // Debug capture?
    if (/*fullscreen && */debugging && CheckKeyPressed(G64_VK_INSERT, 0x0001))
    {
      _debugger.capture = 1;
    }
  }
}

/******************************************************************
Function: ViStatusChanged
Purpose:  This function is called to notify the dll that the
ViStatus registers value has been changed.
input:    none
output:   none
*******************************************************************/
void CALL ViStatusChanged (void)
{
}

/******************************************************************
Function: ViWidthChanged
Purpose:  This function is called to notify the dll that the
ViWidth registers value has been changed.
input:    none
output:   none
*******************************************************************/
void CALL ViWidthChanged (void)
{
}

#ifdef WINPROC_OVERRIDE
LRESULT CALLBACK WndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg)
  {
  case WM_ACTIVATEAPP:
    if (wParam == TRUE && !fullscreen) rdp.window_changed = TRUE;
    break;
  case WM_PAINT:
    if (!fullscreen) rdp.window_changed = TRUE;
    break;

    /*    case WM_DESTROY:
    SetWindowLong (gfx.hWnd, GWL_WNDPROC, (long)oldWndProc);
    break;*/
  }

  return CallWindowProc(oldWndProc, hwnd, msg, wParam, lParam);
}
#endif

int CheckKeyPressed(int key, int mask)
{
static Glide64Keys g64Keys;
#ifdef __WINDOWS__
  return (GetAsyncKeyState(g64Keys[key]) & mask);
#else
  if (grKeyPressed)
    return grKeyPressed(g64Keys[key]);
#endif
  return 0;
}


#ifdef ALTTAB_FIX
int k_ctl=0, k_alt=0, k_del=0;

LRESULT CALLBACK LowLevelKeyboardProc(int nCode,
                                      WPARAM wParam, LPARAM lParam)
{
  if (!fullscreen) return CallNextHookEx(NULL, nCode, wParam, lParam);

  int TabKey = FALSE;

  PKBDLLHOOKSTRUCT p;

  if (nCode == HC_ACTION)
  {
    switch (wParam) {
case WM_KEYUP:    case WM_SYSKEYUP:
  p = (PKBDLLHOOKSTRUCT) lParam;
  if (p->vkCode == 162) k_ctl = 0;
  if (p->vkCode == 164) k_alt = 0;
  if (p->vkCode == 46) k_del = 0;
  goto do_it;

case WM_KEYDOWN:  case WM_SYSKEYDOWN:
  p = (PKBDLLHOOKSTRUCT) lParam;
  if (p->vkCode == 162) k_ctl = 1;
  if (p->vkCode == 164) k_alt = 1;
  if (p->vkCode == 46) k_del = 1;
  goto do_it;

do_it:
  TabKey =
    ((p->vkCode == VK_TAB) && ((p->flags & LLKHF_ALTDOWN) != 0)) ||
    ((p->vkCode == VK_ESCAPE) && ((p->flags & LLKHF_ALTDOWN) != 0)) ||
    ((p->vkCode == VK_ESCAPE) && ((GetKeyState(VK_CONTROL) & 0x8000) != 0)) ||
    (k_ctl && k_alt && k_del);

  break;
    }
  }

  if (TabKey)
  {
    k_ctl = 0;
    k_alt = 0;
    k_del = 0;
    ReleaseGfx ();
  }

  return CallNextHookEx(NULL, nCode, wParam, lParam);
}
#endif
