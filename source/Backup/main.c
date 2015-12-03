/* 
    (c) 2011 Hermes/Estwald <www.elotrolado.net>
    IrisManager (HMANAGER port) (c) 2011 D_Skywalk <http://david.dantoine.org>

    HMANAGER4 is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    HMANAGER4 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    apayloadlong with HMANAGER4.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

#include <lv2/process.h>
#include <sys/file.h>
#include <ppu-lv2.h>
#include <sys/stat.h>
#include <lv2/sysfs.h>

#include <sysutil/disc.h>

#include <sysmodule/sysmodule.h>
#include <pngdec/pngdec.h>
#include <jpgdec/jpgdec.h>

#include <io/pad.h>

#include <tiny3d.h>
#include "libfont2.h"
#include "language.h"
#include "syscall8.h"
#include "payload.h"
#include "payload341/payload_341.h"
#include "payload355/payload_355.h"
#include "payload355dex/payload_355dex.h"
#include "payload421/payload_421.h"
#include "payload421dex/payload_421dex.h"
#include "payload430/payload_430.h"
#include "payload431/payload_431.h"
#include "payload430dex/payload_430dex.h"
#include "payload440/payload_440.h"
#include "payload441/payload_441.h"
#include "payload441dex/payload_441dex.h"
#include "payload446/payload_446.h"

#include "spu_soundmodule.bin.h" // load SPU Module
#include "spu_soundlib.h"

#include <gcmodplay.h>
#include "credits.h"
#include "main.h"

#include "ttf_render.h"
#include "gfx.h"
#include "utils.h"
#include "pad.h"

#include "ftp.h"
#include "psx.h"
#include "archive_manager.h"
#include "controlfan.h"
#include "sysregistry.h"
#include "osk_input.h"

// include fonts

// font 2: 224 chr from 32 to 255, 16 x 32 pix 2 bit depth
#include "font_b.h"

#include "bluray_png_bin.h"
#include "direct_png_bin.h"
#include "usb_png_bin.h"
#include "missing_png_bin.h"
#include "ftp_png_bin.h"
#include "psone_png_bin.h"
#include "psoneiso_png_bin.h"
#include "folder_png_bin.h"
#include "file_png_bin.h"
#include "pkg_png_bin.h"
#include "self_png_bin.h"
#include "space_jpg_bin.h"

#include "music1_mod_bin.h"
#include "music2_mod_bin.h"
#include "music3_mod_bin.h"

#include "cricket_raw_bin.h"

#include "ps3_discless.h"

#include "event_threads.h"

u64 restore_syscall8[2]= {0,0};

int gui_mode = 0; // GUI selector
int sort_mode = 0;

int options_locked = 0; // to 1 when control parental < 9 and not 0 (Disabled)

int noBDVD = 0;
int stops_BDVD = 1;
int bdvd_is_usb = 0;

int is_ps3game_running = 0;

int mode_homebrew = 0;
int game_list_category = 0;
int homelaun = 0;
int ftp_inited = 0;

#define LAUCHMODE_TOCHECK -1
#define LAUCHMODE_CHECKED -2
#define LAUCHMODE_STARTED 0
static int autolaunch = LAUCHMODE_TOCHECK;

static int psx_inserted = 0;

u64 syscall_base = 0ULL;

int (*lv2_unpatch_bdvdemu)(void)= NULL;
int (*lv2_patch_bdvdemu)(uint32_t flags) = NULL;
int (*lv2_patch_storage)(void) = NULL;
int (*lv2_unpatch_storage)(void) = NULL;

void UTF8_to_Ansi(char *utf8, char *ansi, int len); // from osk_input

u16 * ttf_texture;
int update_title_utf8 = 1;
u8 string_title_utf8[128] = "";
int width_title_utf8 = 0;


// music
char * music[6] = {
            (char *) music1_mod_bin, 
            (char *) music2_mod_bin, 
            (char *) music3_mod_bin,
            "Music: Xenon II ( Original SoundTrack Remix )",
            "Music: Xenon II ( Original SoundTrack )",
            "Music: Turrican II ( Original SoundTrack )"};

int song_selected = 0;
#define MAX_SONGS 3

// SPU
u32 spu = 0;
sysSpuImage spu_image;
#define SPU_SIZE(x) (((x)+127) & ~127)

MODPlay mod_track;

u64 frame_count = 0;

#define ROT_INC(x ,y , z) {x++; if(x > y) x = z;}
#define ROT_DEC(x ,y , z) {x--; if(x < y) x = z;}

int menu_screen = 0;
int mode_favourites = 1;

t_directories directories[MAX_DIRECTORIES];

int ndirectories = 0;

int currentdir = 0;
int currentgamedir = 0;
int int_currentdir = 0;

void unpatch_bdvdemu();
int patch_bdvdemu(u32 flags);
int move_origin_to_bdemubackup(char *path);
int move_bdemubackup_to_origin(u32 flags);


u8 * png_texture = NULL;
PngDatas Png_datas[24];
u32 Png_offset[24];
int Png_iscover[24];

PngDatas Png_res[24];
u32 Png_res_offset[24];

int Png_index[24];

extern char * language[];
char self_path[MAXPATHLEN]= "/"__MKDEF_MANAGER_FULLDIR__;

char temp_buffer[8192];

int LoadPNG(PngDatas *png, const char *filename)
{
    int ret;
    pngData png2;

    if(filename) ret = pngLoadFromFile(filename, &png2);
    else ret= pngLoadFromBuffer((const void *) png->png_in, png->png_size, &png2);

    png->bmp_out = png2.bmp_out;
    png->wpitch  = png2.pitch;
    png->width   = png2.width;
    png->height  = png2.height;

    return ret;
}

int LoadJPG(JpgDatas *jpg, char *filename)
{
    int ret;

    jpgData jpg2;

    if(filename) ret = jpgLoadFromFile(filename, &jpg2);
    else ret= jpgLoadFromBuffer((const void *) jpg->jpg_in, jpg->jpg_size, &jpg2);

    jpg->bmp_out = jpg2.bmp_out;
    jpg->wpitch  = jpg2.pitch;
    jpg->width   = jpg2.width;
    jpg->height  = jpg2.height;

    return ret;
}

void Load_PNG_resources()
{
    int i;

    for(i = 0; i < 24; i++) Png_res[i].png_in = NULL;
    for(i = 0; i < 24; i++) {Png_iscover[i] = 0; Png_index[i] = i;}

    // datas for PNG from memory

    Png_res[0].png_in   = (void *) bluray_png_bin;
    Png_res[0].png_size = bluray_png_bin_size;

    Png_res[1].png_in   = (void *) usb_png_bin;
    Png_res[1].png_size = usb_png_bin_size;

    Png_res[2].png_in   = (void *) missing_png_bin;
    Png_res[2].png_size = missing_png_bin_size;

    Png_res[3].png_in   = (void *) direct_png_bin;
    Png_res[3].png_size = direct_png_bin_size;

    Png_res[4].png_in   = (void *) ftp_png_bin;
    Png_res[4].png_size = ftp_png_bin_size;

    Png_res[5].png_in   = (void *) psone_png_bin;
    Png_res[5].png_size = psone_png_bin_size;

    Png_res[6].png_in   = (void *) psoneiso_png_bin;
    Png_res[6].png_size = psoneiso_png_bin_size;

    Png_res[7].png_in   = (void *) folder_png_bin;
    Png_res[7].png_size = folder_png_bin_size;

    Png_res[8].png_in   = (void *) file_png_bin;
    Png_res[8].png_size = file_png_bin_size;

    Png_res[9].png_in   = (void *) pkg_png_bin;
    Png_res[9].png_size = pkg_png_bin_size;

    Png_res[10].png_in   = (void *) self_png_bin;
    Png_res[10].png_size = self_png_bin_size;

  
    // load PNG from memory

    for(i = 0; i < 16; i++)
        if(Png_res[i].png_in != NULL)
            LoadPNG(&Png_res[i], NULL);

    Png_res[16].png_in   = (void *) space_jpg_bin;
    Png_res[16].png_size = space_jpg_bin_size;

    LoadJPG((JpgDatas *) &Png_res[16], NULL);

}

static PngDatas my_png_datas;

int LoadTexturePNG(char * filename, int index)
{
    
    u32 * texture_pointer2 = (u32 *) (png_texture + (index >= 12 ? index : Png_index[index]) * 4096 * 1024); // 4 MB reserved for PNG index

    // here you can add more textures using 'texture_pointer'. It is returned aligned to 16 bytes
   
    memset(&my_png_datas, 0, sizeof(PngDatas));
	if(LoadPNG(&my_png_datas, filename) <0) memset(&my_png_datas, 0, sizeof(PngDatas));
 
    Png_offset[index] = 0;
    memcpy(&Png_datas[index], &my_png_datas, sizeof(PngDatas));
       
    if(my_png_datas.bmp_out) {

        if(index < 12 && my_png_datas.wpitch * my_png_datas.height > 4096 * 1024) { //  too big!
            memset(texture_pointer2, 0, 64 * 64 * 4);
            my_png_datas.wpitch = 64 * 4;
            my_png_datas.height = my_png_datas.width = 64;
        } else
            memcpy(texture_pointer2, my_png_datas.bmp_out, my_png_datas.wpitch * my_png_datas.height);
        
        free(my_png_datas.bmp_out);

        my_png_datas.bmp_out= texture_pointer2;

        memcpy(&Png_datas[index], &my_png_datas, sizeof(PngDatas));
        Png_offset[index] = tiny3d_TextureOffset(my_png_datas.bmp_out);      // get the offset (RSX use offset instead address)

     return 0;
     } else {

         // fake PNG
        my_png_datas.bmp_out= texture_pointer2;

        int n;
        u32 * text = texture_pointer2;

        my_png_datas.width = my_png_datas.height = 64;
        
        my_png_datas.wpitch = my_png_datas.width * 4;
       
        for (n = 0; n < my_png_datas.width * my_png_datas.height; n++) *text++ = 0xff000000;

        memcpy(&Png_datas[index], &my_png_datas, sizeof(PngDatas));

        Png_offset[index] = tiny3d_TextureOffset(my_png_datas.bmp_out);
       
     }

    return -1;
}

int LoadTextureJPG(char * filename, int index)
{
    
    u32 * texture_pointer2 = (u32 *) (png_texture + (index >= 12 ? index : Png_index[index]) * 4096 * 1024); // 4 MB reserved for PNG index

    // here you can add more textures using 'texture_pointer'. It is returned aligned to 16 bytes
   
    
    memset(&my_png_datas, 0, sizeof(PngDatas));

   
	if(LoadJPG((JpgDatas *)&my_png_datas, filename) <0) memset(&my_png_datas, 0, sizeof(PngDatas));
 
    Png_offset[index] = 0;
    memcpy(&Png_datas[index], &my_png_datas, sizeof(PngDatas));
       
    if(my_png_datas.bmp_out) {
        
        if(index < 12 && my_png_datas.wpitch * my_png_datas.height > 4096 * 1024) { //  too big!
            memset(texture_pointer2, 0, 64 * 64 * 4);
            my_png_datas.wpitch = 64 * 4;
            my_png_datas.height = my_png_datas.width = 64;
        } else
            memcpy(texture_pointer2, my_png_datas.bmp_out, my_png_datas.wpitch * my_png_datas.height);
        
        free(my_png_datas.bmp_out);

        my_png_datas.bmp_out= texture_pointer2;

        memcpy(&Png_datas[index], &my_png_datas, sizeof(PngDatas));
        Png_offset[index] = tiny3d_TextureOffset(my_png_datas.bmp_out);      // get the offset (RSX use offset instead address)

     return 0;
     } else {

         // fake PNG
        my_png_datas.bmp_out= texture_pointer2;

        int n;
        u32 * text = texture_pointer2;

        my_png_datas.width = my_png_datas.height = 64;
        
        my_png_datas.wpitch = my_png_datas.width * 4;
       
        for (n = 0; n < my_png_datas.width * my_png_datas.height; n++) *text++ = 0xff000000;
        
        memcpy(&Png_datas[index], &my_png_datas, sizeof(PngDatas));

        Png_offset[index] = tiny3d_TextureOffset(my_png_datas.bmp_out);
       
     }

    return -1;
}

char path_name[MAXPATHLEN];

const char folder_mode[2][16]={{"/"},{"/PS3_GAME/"}};

// no inline!
int get_icon(char * path, const int num_dir)
{
    struct stat s;
    
    if(directories[num_dir].flags & (1<<23)) { // add PSX iso
        sprintf(path, "%s/COVER.JPG", directories[num_dir].path_name);
        if(stat(path, &s)<0) {sprintf(path, "%s/cover.jpg", directories[num_dir].path_name);if(stat(path, &s)<0) return 0; else return 2;}
        else return 2;
     }

    sprintf(path, "%s/COVERS/%s.PNG", self_path, directories[num_dir].title_id);
    if(stat(path, &s)<0) {
       sprintf(path, "%s/covers/%c%c%c%c%s.JPG", self_path, directories[num_dir].title_id[0], directories[num_dir].title_id[1],
            directories[num_dir].title_id[2], directories[num_dir].title_id[3], &directories[num_dir].title_id[5]);
       // get covers from GAMES or GAME
       if(stat(path, &s)<0) {
           sprintf(path, "/dev_hdd0/GAMES/covers/%c%c%c%c%s.JPG", directories[num_dir].title_id[0], directories[num_dir].title_id[1],
                directories[num_dir].title_id[2], directories[num_dir].title_id[3], &directories[num_dir].title_id[5]);
           if(stat(path, &s)<0) sprintf(path, "/dev_hdd0/GAMEZ/covers/%c%c%c%c%s.JPG", directories[num_dir].title_id[0], directories[num_dir].title_id[1],
                directories[num_dir].title_id[2], directories[num_dir].title_id[3], &directories[num_dir].title_id[5]);

       }
    }

    if(stat(path, &s)<0)
    {
        sprintf(path, "%s%sICON0.PNG", directories[num_dir].path_name, &folder_mode[!((directories[num_dir].flags>>31) & 1)][0]);
        return 0;
    }
    else
        return 1;

}

static volatile int break_get_games = 0;

void get_games_2(void *empty) 
{
    int n, f;

    stops_BDVD = 1;

    if(int_currentdir < 0 ||  int_currentdir >= ndirectories) int_currentdir = 0;

    if(mode_favourites) {
        for(f = 0; f < 3; f++) // priority loop
        for(n = 0; n < 12; n++) {

            if(break_get_games) return;
        
            if(favourites.list[n].index < 0 || favourites.list[n].title_id[0] == 0 || favourites.list[n].index >= ndirectories) Png_offset[n] = 0;
            else {

                if(!Png_offset[n]) {

                    if(f == 0 && !(directories[favourites.list[n].index].flags & 1)) continue; // HDD Only
                    if(f == 1 && (directories[favourites.list[n].index].flags & 2049)) continue; // USB Only
                    if(f == 2 && !(directories[favourites.list[n].index].flags & 2048)) continue; // BDVD Only


                    Png_iscover[n] = get_icon(path_name, favourites.list[n].index);
                    
                    if(!strncmp(path_name + strlen(path_name) -4, ".JPG", 4) || !strncmp(path_name + strlen(path_name) -4, ".jpg", 4))
                        LoadTextureJPG(path_name, n);
                    else
                        if(LoadTexturePNG(path_name, n) < 0) ;//Png_offset[n] = 0;
                }

            }
        }
    
    return;
    }

    for(f = 0; f < 3; f++) // priority loop
    for(n = 0; n < 12; n++) {
        if(break_get_games) return;
        if((int_currentdir + n) < ndirectories) {
            
            if(!Png_offset[n]) {

                if(f == 0 && !(directories[(int_currentdir + n)].flags & 1)) continue; // HDD Only
                if(f == 1 && (directories[(int_currentdir + n)].flags & 2049)) continue; // USB Only
                if(f == 2 && !(directories[(int_currentdir + n)].flags & 2048)) continue; // BDVD Only

                Png_iscover[n] = get_icon(path_name, (int_currentdir + n));

                if(!strncmp(path_name + strlen(path_name) -4, ".JPG", 4) || !strncmp(path_name + strlen(path_name) -4, ".jpg", 4))
                        LoadTextureJPG(path_name, n);
                    else
                        if(LoadTexturePNG(path_name, n) < 0) ;

            }
            
        } else Png_offset[n] = 0;
    }

}


void get_games_3(u64 var) 
{

    int n;
    int indx;

    if(var == 1ULL) {
        indx = Png_index[0];
     
        for(n = 0; n < 11; n++){
            Png_iscover[n] = Png_iscover[n + 1];
            Png_offset[n] = Png_offset[n + 1]; 
            Png_datas[n] = Png_datas[n + 1];
            Png_index[n] = Png_index[n + 1];
            if(Png_offset[n]) {
               
            } else {Png_iscover[n] =0; Png_offset[n] = 0;}
        }

        Png_iscover[11] =0; Png_offset[11] = 0; Png_index[11] = indx;

    } else {
        indx = Png_index[11];
        for(n = 11; n > 0; n--){
            Png_iscover[n] = Png_iscover[n - 1];
            Png_offset[n] = Png_offset[n - 1]; 
            Png_datas[n] = Png_datas[n - 1];
            Png_index[n] = Png_index[n - 1];
            if(Png_offset[n]) {
               
            } else {Png_iscover[n] =0; Png_offset[n] = 0;}
        }

        Png_iscover[0] =0; Png_offset[0] = 0;  Png_index[0] = indx;
    }

    get_games_2(NULL);
 }


void get_games()
{
    int n;

    break_get_games = 1; // for short wait
    wait_event_thread(); // wait previous event thread function
    break_get_games = 0;

    int_currentdir = currentdir;

    // reset icon datas
    for(n = 0; n < 12; n++) {Png_iscover[n] =0; Png_offset[n] = 0; Png_index[n] = n;}
    // program new event thread function
    event_thread_send(0x555ULL, (u64) get_games_2, 0); 
}


void DrawCenteredBar2D(float y, float w, float h, u32 rgba)
{
    float x = (848.0f - w)/ 2.0f;

    tiny3d_SetPolygon(TINY3D_QUADS);

    tiny3d_VertexPos(x    , y    , 1.0f);
    tiny3d_VertexColor(rgba);

    tiny3d_VertexPos(x + w, y    , 1.0f);

    tiny3d_VertexPos(x + w, y + h, 1.0f);

    tiny3d_VertexPos(x    , y + h, 1.0f);
    tiny3d_End();
}


void LoadTexture()
{
    int i;

    u32 * texture_mem = tiny3d_AllocTexture(100*1024*1024); // alloc 100MB of space for textures (this pointer can be global)    

    u32 * texture_pointer; // use to asign texture space without changes texture_mem

    if(!texture_mem) return; // fail!

    texture_pointer = texture_mem;

    ResetFont();

    //debug font
    texture_pointer = (u32 *) AddFontFromBitmapArray((u8 *) font_b, (u8 *) texture_pointer, 32, 255, 16, 32, 2, BIT0_FIRST_PIXEL);

/*
    TTFLoadFont(NULL, (void *) comfortaa_ttf_bin, comfortaa_ttf_bin_size);
    texture_pointer = (u32 *) AddFontFromTTF((u8 *) texture_pointer, 32, 255, 32, 32, TTF_to_Bitmap);
    texture_pointer = (u32 *) AddFontFromTTF((u8 *) texture_pointer, 32, 255, 20, 20, TTF_to_Bitmap);
    TTFUnloadFont();


    //new button font
    TTFLoadFont(NULL, (void *) comfortaa_bold_ttf_bin, comfortaa_bold_ttf_bin_size);
    texture_pointer = (u32 *) AddFontFromTTF((u8 *) texture_pointer, 32, 255, 24, 24, TTF_to_Bitmap);
    TTFUnloadFont();
*/
    
    {
        struct stat s;
        sprintf(temp_buffer, "%s/font.ttf", self_path);
        
        if(stat(temp_buffer, &s)<0 || TTFLoadFont(0, temp_buffer, NULL, 0)!=0) {//
            if(TTFLoadFont(0, "/dev_flash/data/font/SCE-PS3-SR-R-JPN.TTF", NULL, 0)!=0)
                if(TTFLoadFont(0, "/dev_flash/data/font/SCE-PS3-NR-R-JPN.TTF", NULL, 0)!=0)
                    exit(0);
                //TTFLoadFont(NULL, (void *) comfortaa_bold_ttf_bin, comfortaa_bold_ttf_bin_size);
        }
    

        TTFLoadFont(1, "/dev_flash/data/font/SCE-PS3-DH-R-CGB.TTF", NULL, 0);
        TTFLoadFont(2, "/dev_flash/data/font/SCE-PS3-SR-R-LATIN2.TTF", NULL, 0);
        TTFLoadFont(3, "/dev_flash/data/font/SCE-PS3-YG-R-KOR.TTF", NULL, 0);
    

    }
    

    Load_PNG_resources();

    for(i = 0; i < 24; i++) {
       
        if(Png_res[i].png_in == NULL) continue;

        Png_res_offset[i]   = 0;
       
        if(Png_res[i].bmp_out) {

            memcpy(texture_pointer, Png_res[i].bmp_out, Png_res[i].wpitch * Png_res[i].height);
            
            free(Png_res[i].bmp_out); // free the PNG because i don't need this datas

            Png_res_offset[i] = tiny3d_TextureOffset(texture_pointer);      // get the offset (RSX use offset instead address)

            texture_pointer += ((Png_res[i].wpitch * Png_res[i].height + 15) & ~15) / 4; // aligned to 16 bytes (it is u32) and update the pointer
         }
    }


    ttf_texture = (u16 *) texture_pointer;

    texture_pointer += 1024 * 16;

    texture_pointer = (u32 *) init_ttf_table((u16 *) texture_pointer);

    png_texture = (u8 *) texture_pointer;
}


int background_sel = 0;

u32 background_colors[8] = {
    0xff80804f,
    0xff10000F,
    0xff606060,
    0xff904f80,
    // new hermes colors
    0xff0040cf,
    0xff80804f,
    0xff300060,
    0xff904f80,
};

u32 background_colors2[16] = {
    0x0040A8ff,
    0x001048ff,

    0x040404ff,
    0x030303ff,

    0xA840A8ff,
    0x481048ff,

    0xf8b010ff,
    0x481008ff,

    0x2040B8ff,
    0x102048ff,

   // 0x206F00ff,
   // 0x004000ff,

    //0xFFA838ff,
    //0x785418ff,
    //0xBAD7A8FF,
    0x94AC86FF,
    0x5D6B54FF,

    0x784078ff,
    0x281028ff,

    0x86AC94FF,
    0x546B5DFF,

};


void cls0()
{
    cls2();

    u32 color1; //0x002088ff;
    u32 color2;

    color1 = background_colors2[(background_sel & 7) << 1];
    color2 = background_colors2[((background_sel & 7) << 1) + 1];

    tiny3d_SetPolygon(TINY3D_QUADS);
    
    tiny3d_VertexPos(-80    , -80    , 1000);
    tiny3d_VertexColor(color1);

    tiny3d_VertexPos(938  , -80    , 1000);
    tiny3d_VertexColor(color1);

    tiny3d_VertexPos(938  , 400  , 1000);
    tiny3d_VertexColor(color2);

    tiny3d_VertexPos(-80    , 400  , 1000);
    tiny3d_VertexColor(color2);


    tiny3d_VertexPos(-80    , 400    , 1000);
    tiny3d_VertexColor(color2);

    tiny3d_VertexPos(938  , 400    , 1000);
    tiny3d_VertexColor(color2);

    tiny3d_VertexPos(938  , 592  , 1000);
    tiny3d_VertexColor(0x080008ff);

    tiny3d_VertexPos(-80    , 592  , 1000);
    tiny3d_VertexColor(0x080008ff);

    tiny3d_End();

}

void cls()
{
    
    if(gui_mode == 1)  {cls0();return;}
    tiny3d_Clear(background_colors[background_sel & 7], TINY3D_CLEAR_ALL);

        
    // Enable alpha Test
    tiny3d_AlphaTest(1, 0x10, TINY3D_ALPHA_FUNC_GEQUAL);

    // Enable alpha blending.
            tiny3d_BlendFunc(1, TINY3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA,
                TINY3D_BLEND_FUNC_DST_RGB_ONE_MINUS_SRC_ALPHA | TINY3D_BLEND_FUNC_DST_ALPHA_ZERO,
                TINY3D_BLEND_RGB_FUNC_ADD | TINY3D_BLEND_ALPHA_FUNC_ADD);
    reset_ttf_frame();
}

void cls2()
{
    tiny3d_Clear(0xff000000, TINY3D_CLEAR_ALL);
        
    // Enable alpha Test
    tiny3d_AlphaTest(1, 0x10, TINY3D_ALPHA_FUNC_GEQUAL);

   // Enable alpha blending.
            tiny3d_BlendFunc(1, TINY3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA,
                TINY3D_BLEND_FUNC_DST_RGB_ONE_MINUS_SRC_ALPHA | TINY3D_BLEND_FUNC_DST_ALPHA_ZERO,
                TINY3D_BLEND_RGB_FUNC_ADD | TINY3D_BLEND_ALPHA_FUNC_ADD);
    reset_ttf_frame();
}

/******************************************************************************************************************************************************/
/* Payload functions                                                                                                                                  */
/******************************************************************************************************************************************************/

static int sys_ss_media_id(void * id)
{
    lv2syscall2(879, 0x10001ULL, (u64) id);

    return_to_user_prog(int);
    
}

static u64 syscall_40(u64 cmd, u64 arg)
{
    lv2syscall2(40, cmd, arg);

    return_to_user_prog(u64);
    
}


int sys_set_leds(u64 color, u64 state) 
{
    lv2syscall2(386,  (u64) color, (u64) state);
    return_to_user_prog(int);
}

int sys_game_get_temperature(int sel, u32 *temperature) 
{
    u32 temp;
  
    lv2syscall2(383, (u64) sel, (u64) &temp); 
    *temperature = (temp >> 24);
    return_to_user_prog(int);
}

int sys_fs_mount(char const* deviceName, char const* deviceFileSystem, char const* devicePath, int writeProt) 
{
    lv2syscall8(837, (u64) deviceName, (u64) deviceFileSystem, (u64) devicePath, 0, (u64) writeProt, 0, 0, 0 );
    return_to_user_prog(int);
}

int sys_fs_umount(char const* devicePath) 
{
    lv2syscall3(838,  (u64) devicePath, 0, 0 );
    return_to_user_prog(int);
}

u64 lv2peek(u64 addr) 
{ 
    lv2syscall1(6, (u64) addr);
    return_to_user_prog(u64);

}

u64 lv2poke(u64 addr, u64 value) 
{ 
    lv2syscall2(7, (u64) addr, (u64) value); 
    return_to_user_prog(u64);
}


int lv2launch(u64 addr) 
{ 
    lv2syscall8(9, (u64) addr, 0,0,0,0,0,0,0); 
    return_to_user_prog(int);
}

/*
int syscall36(char * path) 
{ 
    lv2syscall1(36, (u64) path);
    return_to_user_prog(int);
    
}
*/

u64 hmanager_key = 0x1759829723742374ULL;

/******************************************************************************************************************************************************/

// manager


char payload_str[256];

int videoscale_x = 0;
int videoscale_y = 0;

int flash;

int select_px = 0;
int select_py = 0;

u32 blockSize;
u64 freeSize;
float freeSpace[12];

int select_option = 0;

u32 fdevices=0;
u32 fdevices_old=0;
u32 forcedevices=0;
int find_device=0;

char hdd_folder[64]="12345";

char bluray_game[64]; // name of the game

static int exit_program = 0;

#define ROUND_UP12(x) ((((x)+11)/12)*12)

void draw_gui1(float x, float y);
void draw_gui2(float x, float y);
void gui_control();
void draw_options(float x, float y, int index);
void draw_configs(float x, float y, int index);
void draw_gbloptions(float x, float y);
void draw_toolsoptions(float x, float y);
void draw_cachesel(float x, float y);
void draw_pkginstall(float x, float y);

struct {
    int videoscale_x[4];
    int videoscale_y[4];
    int background_sel;
    char hdd_folder[64];
    u32 usekey;
    char pad[156];
} manager_oldcfg;

struct {
    int videoscale_x[4];
    int videoscale_y[4];
    int background_sel;
    char hdd_folder[64];
    u32 usekey;
    u8 language;
    u8 noBDVD;
    u8 gui_mode;
    char pad[149];
    u32 event_flag;
    u32 opt_flags;
} manager_cfg;

struct {
    int version;
    int perm;
    int useBDVD;
    int updates;
    int ext_ebootbin;
    int bdemu;
    int exthdd0emu;
    int direct_boot;
    int bdemu_ext;
    int pad[5];
} game_cfg;


int inited = 0;

#define INITED_SPU          2
#define INITED_SOUNDLIB     4
#define INITED_GCM_SYS      8
#define INITED_IO          16
#define INITED_PNGDEC      32
#define INITED_FS          64
#define INITED_JPGDEC      128
#define INITED_MODLIB      256

int set_install_pkg = 0;

int sys_shutdown()
{   
    unlink_secure("/dev_hdd0/tmp/turnoff");
    
    lv2syscall4(379,0x1100,0,0,0);
    return_to_user_prog(int);
}

static int sys_reboot()
{
    lv2syscall4(379,0x1200,0,0,0);
    return_to_user_prog(int);
}

void fun_exit()
{
    // multiple calls? (Yes!)
    static int one = 1;
    if(!one) return;
    one = 0;

    event_threads_finish();
    set_usleep_sm_main(250000);

    if(!is_ps3game_running && lv2peek(0x80000000000004E8ULL)) syscall_40(1, 0); // disables PS3 Disc-less

    close_language();
    TTFUnloadFont();

    ftp_deinit();

    if(inited & INITED_SOUNDLIB) {
        if(inited & INITED_MODLIB)  
            MODPlay_Unload (&mod_track);
        SND_End();

        
    }

    if(inited & INITED_SPU) {
        sleep(1);
        sysSpuRawDestroy(spu);
        sysSpuImageClose(&spu_image);
    }

    if(inited & INITED_GCM_SYS) sysModuleUnload(SYSMODULE_GCM_SYS);
    if(inited & INITED_IO)      sysModuleUnload(SYSMODULE_IO);
    if(inited & INITED_PNGDEC)  sysModuleUnload(SYSMODULE_PNGDEC);
    if(inited & INITED_JPGDEC)  sysModuleUnload(SYSMODULE_JPGDEC);
    if(inited & INITED_FS)      sysModuleUnload(SYSMODULE_FS);
    sysModuleUnload(SYSMODULE_SYSUTIL);
    
    inited = 0;
    //if(manager_cfg.usekey) sys8_disable(hmanager_key);
    
    ioPadEnd();
    
    if(lv2_unpatch_storage) lv2_unpatch_storage();
    
    if(set_install_pkg){

        unlink_secure("/dev_hdd0/tmp/turnoff");
    }

    sys8_perm_mode(0); // perms to 0 from exit()

    if(restore_syscall8[0]) sys8_pokeinstr(restore_syscall8[0], restore_syscall8[1]);
    
    if(game_cfg.direct_boot == 555)
        sysProcessExitSpawn2("/dev_bdvd/PS3_GAME/USRDIR/EBOOT.BIN", NULL, NULL, NULL, 0, 3071, SYS_PROCESS_SPAWN_STACK_SIZE_1M);

    if(set_install_pkg) sys_reboot();

}

void LoadManagerCfg()
{

    sprintf(temp_buffer, "%s/config/manager_setup.bin", self_path);

    int file_size;
    char *file = LoadFile(temp_buffer, &file_size);

    if(file)
    {
        if(file_size != sizeof(manager_cfg))
        {
            file_size = sizeof(manager_oldcfg); // safe
            manager_cfg.opt_flags |= OPTFLAGS_PLAYMUSIC; // enabled by default
        }

        memcpy(&manager_cfg, file, file_size);
        free(file);
    }
    else
    {
        manager_cfg.opt_flags |= OPTFLAGS_PLAYMUSIC; // enabled by default
    }

    background_sel = manager_cfg.background_sel & 7;

    switch(Video_Resolution.height) {
        case 480:
            videoscale_x = manager_cfg.videoscale_x[0];
            videoscale_y = manager_cfg.videoscale_y[0];
            break;
       case 576:
            videoscale_x = manager_cfg.videoscale_x[1];
            videoscale_y = manager_cfg.videoscale_y[1];
            break;
       case 720:
            videoscale_x = manager_cfg.videoscale_x[2];
            videoscale_y = manager_cfg.videoscale_y[2];
            break;
       default:
            videoscale_x = manager_cfg.videoscale_x[3];
            videoscale_y = manager_cfg.videoscale_y[3];
            break;
    }


    
    sprintf(temp_buffer, "%s/config/language.ini", self_path);
    open_language(manager_cfg.language, temp_buffer);
    
}

void auto_ftp(void)
{
    static int one = 1;
    if(!one) return;

    one= 0;
    if (manager_cfg.opt_flags & OPTFLAGS_FTP) // maybe we need add an icon to user...
    {
        int r = ftp_init();

        if(r == 0)
        {
            ftp_inited = 1; //DrawDialogOK("FTP Service init on boot: OK");
            
        } else {
            if(r == -1) DrawDialogOKTimer("Error in netInitialize()", 2000.0f);
            else if(r == -2) DrawDialogOKTimer("Error in netCtlInit()", 2000.0f);
            else if(r == -3) DrawDialogOKTimer("Error in netCtlGetInfo()", 2000.0f);
            else if(r == -4) DrawDialogOKTimer("Net Disconnected or Connection not Established", 2000.0f);
            else DrawDialogOK("FTP Unknown Error");
        }
    }
}

int SaveManagerCfg()
{
    sprintf(temp_buffer, "%s/config/manager_setup.bin", self_path);
    return SaveFile(temp_buffer, (char *) &manager_cfg, sizeof(manager_cfg));
}

void video_adjust()
{
    
    SetCurrentFont(FONT_TTF);
    while(1) {

        double sx = (double) Video_Resolution.width;
        double sy = (double) Video_Resolution.height;
        double psx = (double) (1000 + videoscale_x)/1000.0;
        double psy = (double) (1000 + videoscale_y)/1000.0;
        
        tiny3d_UserViewport(1, 
            (float) ((sx - sx * psx) / 2.0), // 2D position
            (float) ((sy - sy * psy) / 2.0), 
            (float) ((sx * psx) / 848.0),    // 2D scale
            (float) ((sy * psy) / 512.0),
            (float) ((sx / 1920.0) * psx),  // 3D scale
            (float) ((sy / 1080.0) * psy));

        tiny3d_Clear(background_colors[background_sel & 7], TINY3D_CLEAR_ALL);

        
        // Enable alpha Test
        tiny3d_AlphaTest(1, 0x10, TINY3D_ALPHA_FUNC_GEQUAL);

        // Enable alpha blending.
                tiny3d_BlendFunc(1, TINY3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA,
                    TINY3D_BLEND_FUNC_DST_RGB_ONE_MINUS_SRC_ALPHA | TINY3D_BLEND_FUNC_DST_ALPHA_ZERO,
                    TINY3D_BLEND_RGB_FUNC_ADD | TINY3D_BLEND_ALPHA_FUNC_ADD);
        reset_ttf_frame();
        
        DrawAdjustBackground(0xffffffff) ; // light blue 

        update_twat();
        SetFontSize(16, 24);
        SetFontColor(0xffffffff, 0x0);

        SetFontAutoCenter(1);

        DrawFormatString(0, (512 - 24)/2 - 64, "%s", language[VIDEOADJUST_POSITION]);

        DrawFormatString(0, (512 - 24)/2, language[VIDEOADJUST_SCALEINFO], videoscale_x, videoscale_y);

        DrawFormatString(0, (512 - 24)/2 + 64, "%s", language[VIDEOADJUST_EXITINFO]);
       
        DrawFormatString(0, (512 - 24)/2 + 96, "%s", language[VIDEOADJUST_DEFAULTS]);

        // Warning!! don´t traslate this string!
        DrawFormatString(0, (512 - 24)/2 + 128, "%s", "Press [] to English language");

        SetFontAutoCenter(0);

        tiny3d_Flip();

        ps3pad_read();

        if(!(frame_count & 3)) {
            if(old_pad & BUTTON_UP) {if(videoscale_y > -179) videoscale_y--;}
            if(old_pad & BUTTON_DOWN) {if(videoscale_y < 10) videoscale_y++;}
            if(old_pad & BUTTON_LEFT) {if(videoscale_x > -199) videoscale_x--;}
            if(old_pad & BUTTON_RIGHT) {if(videoscale_x < 10) videoscale_x++;}
        }
        
        if(new_pad & BUTTON_SQUARE) {
            manager_cfg.language = 0;
            sprintf(temp_buffer, "%s/config/language.ini", self_path);
            open_language(manager_cfg.language, temp_buffer);
        }

        if(new_pad & BUTTON_CROSS) {

            switch(Video_Resolution.height) {
                case 480:
                    manager_cfg.videoscale_x[0] = videoscale_x;
                    manager_cfg.videoscale_y[0] = videoscale_y;
                    break;
               case 576:
                    manager_cfg.videoscale_x[1] = videoscale_x;
                    manager_cfg.videoscale_y[1] = videoscale_y;
                    break;
               case 720:
                    manager_cfg.videoscale_x[2] = videoscale_x;
                    manager_cfg.videoscale_y[2] = videoscale_y;
                    break;
               default:
                    manager_cfg.videoscale_x[3] = videoscale_x;
                    manager_cfg.videoscale_y[3] = videoscale_y;
                    break;
            }
            
      
            if(SaveManagerCfg() == 0) {
                sprintf(temp_buffer, "manager_setup.bin\n\n%s", language[GLOBAL_SAVED]);
                DrawDialogOK(temp_buffer);
            }

            break;
        }

        if(new_pad & BUTTON_CIRCLE) {videoscale_x = videoscale_y = -120;}

        frame_count++;
    }

}

void Select_games_folder() 
{

    DIR  *dir, *dir2;
    int selected = 0;
    char tmp[256];

    dir = opendir ("/dev_hdd0/GAMES");
    if(dir) {
        closedir (dir);
        sprintf(temp_buffer, "%s %s %s", language[GAMEFOLDER_WANTUSE], "/dev_hdd0/GAMES", language[GAMEFOLDER_TOINSTALLNTR]); 

        if(DrawDialogYesNo(temp_buffer) == 1) {
            strncpy(hdd_folder, "dev_hdd0_2", 64);
            strncpy(manager_cfg.hdd_folder, "dev_hdd0_2", 64); 
            return;
        }
    }
    
    dir = opendir ("/dev_hdd0/"__MKDEF_GAMES_DIR);
    if(dir) {
        closedir (dir);
        sprintf(temp_buffer, "%s %s %s", language[GAMEFOLDER_WANTUSE], "/dev_hdd0/"__MKDEF_GAMES_DIR, language[GAMEFOLDER_TOINSTALLNTR]); 

        if(DrawDialogYesNo(temp_buffer) == 1) {
            strncpy(hdd_folder, "dev_hdd0", 64);
            strncpy(manager_cfg.hdd_folder, "dev_hdd0", 64); 
            return;
        }
    }

    dir = opendir ("/dev_hdd0/game");

    if(dir) {

        while(1) {

            struct dirent *entry = readdir (dir);
          
            if(!entry) break;

            if(entry->d_name[0]=='.') continue;

            if(!(entry->d_type & DT_DIR)) continue;

            sprintf(temp_buffer, "/dev_hdd0/game/%s/"__MKDEF_GAMES_DIR, entry->d_name);
        
            dir2 = opendir (temp_buffer);

            if(dir2) {
                
                closedir (dir2);
      
                sprintf(temp_buffer, "%s /%s %s", language[GAMEFOLDER_WANTUSE], entry->d_name, language[GAMEFOLDER_TOINSTALLNTR]);

                if(DrawDialogYesNo(temp_buffer) == 1) {
                    strncpy(hdd_folder, entry->d_name, 64);
                    strncpy(manager_cfg.hdd_folder, entry->d_name, 64);
                    selected = 1;
                    break;
                }
            }
            
          }
    closedir (dir);
    }

    if(!selected) {
        
        sprintf(temp_buffer, "%s %s %s", language[GAMEFOLDER_WANTUSE], "/dev_hdd0/" __MKDEF_GAMES_DIR, language[GAMEFOLDER_TOINSTALLNTR]);

        if(DrawDialogYesNo(temp_buffer) == 1) {
            strncpy(hdd_folder, "dev_hdd0", 64);
            strncpy(manager_cfg.hdd_folder, "dev_hdd0", 64); 
            mkdir_secure("/dev_hdd0/" __MKDEF_GAMES_DIR);
        } else {
            strncpy(hdd_folder, __MKDEF_MANAGER_DIR__, 64);
            strncpy(manager_cfg.hdd_folder, __MKDEF_MANAGER_DIR__, 64);
            sprintf(tmp, "/%s/" __MKDEF_GAMES_DIR, __MKDEF_MANAGER_FULLDIR__);
            mkdir_secure(tmp);

            sprintf(temp_buffer, "%s %s %s", language[GAMEFOLDER_USING], tmp, language[GAMEFOLDER_TOINSTALL]);
            DrawDialogOK(temp_buffer);
        }
    }
}

void pause_music(int pause)
{
    if((!pause)&&(!(manager_cfg.opt_flags & OPTFLAGS_PLAYMUSIC)))
        return;

    SND_Pause(pause);
}

void init_music()
{
    MODPlay_Init(&mod_track);
    
    int file_size;
    char *file;

    sprintf(temp_buffer, "%s/music.mod", self_path);
    file = LoadFile(temp_buffer, &file_size);

    if(!file) {
   
        sprintf(temp_buffer, "%s/MUSIC.MOD", self_path);
        file = LoadFile(temp_buffer, &file_size);
    }
    
    if(!file) {
        srand(time(0)); // randomize seed
        song_selected = rand() % MAX_SONGS;
        file = (char *) music[song_selected];
    } else {
        // paranoid code to copy the .mod in aligned and large memory

        char *file2 = memalign(32, file_size + 32768);
        if(file2) {memcpy(file2, file, file_size);free(file); file = file2;}
    }

    if(MODPlay_SetMOD (&mod_track, file)<0) {
        MODPlay_Unload (&mod_track);
    } else {
        MODPlay_SetVolume( &mod_track, 6, 6); // fix the volume to 16 (max 64)
	    MODPlay_Start (&mod_track); // Play the MOD
        inited |= INITED_MODLIB;
        SND_Pause(1); //force pause here
    }

}


int payload_mode = 0;

int firmware = 0;

int load_from_bluray = 0;

static char filename[0x420];

/******************************************************************************************************************************************************/

static volatile int bdvd_notify = 0;

static volatile int bdvd_ejected = 1;

static volatile int disc_less_on = 0;

void DiscEjectCallback(void)
{
    bdvd_notify = -1;
    bdvd_ejected = 1;
}

void DiscInsertCallback(u32 discType, char *title)
{
    
    bdvd_notify = 0;

    if(!noBDVD && lv2_patch_storage) {
        
        if(firmware < 0x421C)
            Reset1_BDVD();
        else
            Reset2_BDVD();
    }
    
    if(noBDVD !=2) {
        bdvd_notify = 1;
        bdvd_ejected = 0;

        mode_favourites = 0;
        select_option = 0;     
        menu_screen = 0;
        select_px = 0;
        select_py = 0;
        currentgamedir = currentdir = 0;
    } else disc_less_on = 1;
}

int is_libfs_patched(void){
    struct stat s;
    char path[256];

    sprintf(path, "%s/libfs_patched.sprx", self_path);
    return (stat(path, &s) == 0);
}

                    
/******************************************************************************************************************************************************/

int test_ftp_working()
{
    if(get_ftp_activity()) {
        if(DrawDialogYesNo("FTP is working now\nWhat you interrupt the FTP activity?\n\nEl FTP esta trabajando ahora mismo\nQuieres interrumpir su actividad?") == 1) {
            ftp_deinit();
            ftp_inited = 0;
            return 0;
            
        } else return 1;
    } return 0;
}

int is_payload_loaded(void)
{
    u64 addr = lv2peek(0x80000000000004f0ULL);

    if((addr>>32) == 0x534B3145) {
        addr&= 0xffffffff;
        if(addr && peekq(0x80000000000004f8ULL)) {
            restore_syscall8[0]= lv2peek(0x80000000000004f8ULL); // (8*8)
            restore_syscall8[1]= lv2peek(restore_syscall8[0]);
            lv2poke(restore_syscall8[0], 0x8000000000000000ULL + (u64) (addr + 0x20));
            return 2;
        }
        
        return 1;
    }
   
    return 0;
}

s32 main(s32 argc, const char* argv[])
{
    int n;
    
    u32 entry = 0;
    u32 segmentcount = 0;
    sysSpuSegment * segments;

    if(lv2peek(0x80000000000004E8ULL)) syscall_40(1, 0); // disables PS3 Disc-less 

    event_threads_init();

    atexit(fun_exit);

    int must_patch =0;


    if(lv2peek(0x80000000007EF220ULL)==0x45737477616C6420ULL && is_payload_loaded()) {

        must_patch = 1;

        sys8_path_table(0LL); // break libfs.sprx re-direction

        if(restore_syscall8[0]) sys8_pokeinstr(restore_syscall8[0], restore_syscall8[1]);
    }

    if(sysModuleLoad(SYSMODULE_FS) ==0)      inited|= INITED_FS;      else exit(0);
    if(sysModuleLoad(SYSMODULE_PNGDEC) ==0)  inited|= INITED_PNGDEC;  else exit(0);
    if(sysModuleLoad(SYSMODULE_JPGDEC) ==0)  inited|= INITED_JPGDEC;  else exit(0);
    if(sysModuleLoad(SYSMODULE_IO) ==0)      inited|= INITED_IO;      else exit(0);
    if(sysModuleLoad(SYSMODULE_GCM_SYS) ==0) inited|= INITED_GCM_SYS; else exit(0);

    sysModuleLoad(SYSMODULE_SYSUTIL);
    sysSpuInitialize(6, 5);

    sysSpuRawCreate(&spu, NULL);

    sysSpuElfGetInformation(spu_soundmodule_bin, &entry, &segmentcount);

    size_t segmentsize = sizeof(sysSpuSegment) * segmentcount;
    segments = (sysSpuSegment*)memalign(128, SPU_SIZE(segmentsize)); // must be aligned to 128 or it break malloc() allocations
    memset(segments, 0, segmentsize);

    sysSpuElfGetSegments(spu_soundmodule_bin, segments, segmentcount);
    
    sysSpuImageImport(&spu_image, spu_soundmodule_bin, 0);
    
    sysSpuRawImageLoad(spu, &spu_image);

    inited |= INITED_SPU;

    if(SND_Init(spu)==0) inited |= INITED_SOUNDLIB;

    if(argc>0 && argv) {
    
        if(!strncmp(argv[0], "/dev_hdd0/game/", 15)) {
            int n;

            strcpy(self_path, argv[0]);

            n= 15; while(self_path[n] != '/' && self_path[n] != 0) n++;

            if(self_path[n] == '/') {
                self_path[n] = 0;
            }
        }
    }


    if(is_firm_341()) {
        firmware = 0x341C;
        payload_mode = is_payload_loaded_341();
    } else if(is_firm_355()) {
        firmware = 0x355C;
        payload_mode = is_payload_loaded_355();
    } else if(is_firm_355dex()) {
        firmware = 0x355D; 
        payload_mode = is_payload_loaded_355dex();
	} else if(is_firm_421()) {
        firmware = 0x421C;
        payload_mode = is_payload_loaded_421();
    } else if(is_firm_421dex()) {
        firmware = 0x421D;
        payload_mode = is_payload_loaded_421dex();
    } else if(is_firm_430()) {
        firmware = 0x430C;
        payload_mode = is_payload_loaded_430();
    } else if(is_firm_431()) {
        firmware = 0x431C;
        payload_mode = is_payload_loaded_431();
	} else if(is_firm_430dex()) {
        firmware = 0x430D;
        payload_mode = is_payload_loaded_430dex();
	} else if(is_firm_440()) {
        firmware = 0x440C;
        payload_mode = is_payload_loaded_440();
    } else if(is_firm_441()) {
        firmware = 0x441C;
        payload_mode = is_payload_loaded_441();
	} else if(is_firm_446()) {
        firmware = 0x446C;
        payload_mode = is_payload_loaded_446();
	}
	
	
    
    //sprintf(temp_buffer + 0x1000, "firmware: %xex payload %i", firmware, payload_mode);

    ///////////////////////////

    switch(firmware) {
        case 0x341C:
            set_bdvdemu_341(payload_mode);
            switch(payload_mode)
            {
                case ZERO_PAYLOAD: //no payload installed
                    load_payload_341(payload_mode);
                    __asm__("sync");
                    sleep(1); /* maybe need it, maybe not */
                    break;
        		case HERMES_PAYLOAD:
        		    break;
            }
            break;
        case 0x355C:
            set_bdvdemu_355(payload_mode);
            switch(payload_mode)
            {
                case WANIN_PAYLOAD:
                case ZERO_PAYLOAD: //no payload installed
            		install_new_poke(); /* need for patch lv2 */

            		if (!map_lv1()) {
            			remove_new_poke();

                        tiny3d_Init(1024*1024);
                        ioPadInit(7);
                        DrawDialogOK("Error Loading Payload: map failed?!");
            			exit(0);
            		}

            		patch_lv2_protection(); /* yaw */
            		remove_new_poke(); /* restore pokes */
        
            		unmap_lv1();  /* 3.55 need unmap? */
                    
                    __asm__("sync");
                    sleep(1); /* dont touch! nein! */

                    //please, do not translate this strings - i preffer this errors in english for better support...
                    if(payload_mode == WANIN_PAYLOAD)
                    {
                        sys8_disable_all = 1;
                        sprintf(temp_buffer, "WANINV2 DETECTED\nOLD SYSCALL 36 LOADED (mode=%i)\n\n - no big files allowed with this payload -", payload_mode);
                        sprintf(payload_str, "wanin cfw - old syscall36, no bigfiles allowed");
                    } else {
		                load_payload_355(payload_mode);
                       
                        __asm__("sync");
                        sleep(1); /* maybe need it, maybe not */
                    }
                    break;
		        case SYS36_PAYLOAD:
                    sys8_disable_all = 1;
                    sprintf(temp_buffer, "OLD SYSCALL 36 RESIDENT, RESPECT!\nNEW PAYLOAD NOT LOADED...\n\n - no big files allowed with this payload -");
                    sprintf(payload_str, "syscall36 resident - new payload no loaded, no bigfiles allowed");
                    break;
        		case SKY10_PAYLOAD:
        		    break;
            }
            break;
        case 0x355D: //355dex
			set_bdvdemu_355dex(payload_mode);
			switch(payload_mode)
            {
                case ZERO_PAYLOAD: //no payload installed
					install_new_poke_355dex();
            		if (!map_lv1_355dex()) {
            			remove_new_poke_355dex();

                        tiny3d_Init(1024*1024);
                        ioPadInit(7);
                        DrawDialogOK("Error Loading Payload: map failed?!");
            			exit(0);
            		}
            		patch_lv2_protection_355dex(); /* yaw */
            		
            		remove_new_poke_355dex(); /* restore pokes */
            		unmap_lv1_355dex();  /* 3.55 need unmap? */
                    __asm__("sync");
                    
                    load_payload_355dex(payload_mode);
                    __asm__("sync");
                    sleep(1); /* maybe need it, maybe not */
                    break;
        		case SKY10_PAYLOAD:
        		    break;
            }
            break;
        case 0x421C:
            set_bdvdemu_421(payload_mode);
            switch(payload_mode)
            {
                case ZERO_PAYLOAD: //no payload installed
                    load_payload_421(payload_mode);
                    __asm__("sync");
                    sleep(1); /* maybe need it, maybe not */
                    break;
        		case SKY10_PAYLOAD:
        		    break;
            }
            break;
        case 0x421D: //4.21 dex
			set_bdvdemu_421dex(payload_mode);
            switch(payload_mode)
            {
                case ZERO_PAYLOAD: //no payload installed
                    load_payload_421dex(payload_mode);
                    __asm__("sync");
                    sleep(1); /* maybe need it, maybe not */
                    break;
        		case SKY10_PAYLOAD:
        		    break;
            }
            break;
        case 0x430C:
            set_bdvdemu_430(payload_mode);
            switch(payload_mode)
            {
                case ZERO_PAYLOAD: //no payload installed
                    load_payload_430(payload_mode);
                    __asm__("sync");
                    sleep(1); /* maybe need it, maybe not */
                    break;
        		case SKY10_PAYLOAD:
        		    break;
            }
            break;
        case 0x431C:
            set_bdvdemu_431(payload_mode);
            switch(payload_mode)
            {
                case ZERO_PAYLOAD: //no payload installed
                    load_payload_431(payload_mode);
                    __asm__("sync");
                    sleep(1); /* maybe need it, maybe not */
                    break;
        		case SKY10_PAYLOAD:
        		    break;
            }
            break;
			        case 0x430D:
            set_bdvdemu_430dex(payload_mode);
            switch(payload_mode)
            {
                case ZERO_PAYLOAD: //no payload installed
                    load_payload_430dex(payload_mode);
                    __asm__("sync");
                    sleep(1); /* maybe need it, maybe not */
                    break;
        		case SKY10_PAYLOAD:
        		    break;
            }
            break;
		case 0x440C:
            set_bdvdemu_440(payload_mode);
            switch(payload_mode)
            {
                case ZERO_PAYLOAD: //no payload installed
                    load_payload_440(payload_mode);
                    __asm__("sync");
                    sleep(1); /* maybe need it, maybe not */
                    break;
        		case SKY10_PAYLOAD:
        		    break;
            }
			break;
        case 0x441C:
            set_bdvdemu_441(payload_mode);
            switch(payload_mode)
            {
                case ZERO_PAYLOAD: //no payload installed
                    load_payload_441(payload_mode);
                    __asm__("sync");
                    sleep(1); /* maybe need it, maybe not */
                    break;
        		case SKY10_PAYLOAD:
        		    break;
            }
			break;
        case 0x446C:
            set_bdvdemu_446(payload_mode);
            switch(payload_mode)
            {
                case ZERO_PAYLOAD: //no payload installed
                    load_payload_446(payload_mode);
                    __asm__("sync");
                    sleep(1); /* maybe need it, maybe not */
                    break;
        		case SKY10_PAYLOAD:
        		    break;
            }
			break;
        default:
            tiny3d_Init(1024*1024);
            ioPadInit(7);
            DrawDialogOK("Error: Unsupported firmware!");
            exit(0);
            break;
	}

    usleep(250000);

    if(payload_mode >= ZERO_PAYLOAD && sys8_disable_all == 0)
    {
        int test = 0x100;

        //check syscall8 status
        test = sys8_enable(0ULL);
        if((test & 0xff00) == 0x300)
        {
            if(payload_mode == ZERO_PAYLOAD) {
                if(firmware== 0x341C)
                    sprintf(payload_str, "payload-hermes - new syscall8 v%i (libfs_patched %s)", test & 0xff, is_libfs_patched()? "found!": "not found");
                else 
                    sprintf(payload_str, "payload-sk1e - new syscall8 v%i (libfs_patched %s)", test & 0xff, is_libfs_patched()? "found!": "not found");
            }
            else if (payload_mode == SKY10_PAYLOAD)
                sprintf(payload_str, "payload-sk1e resident - new syscall8 v%i (libfs_patched %s)", test & 0xff, is_libfs_patched()? "found!": "not found");
                else sprintf(payload_str, "payload-hermes resident - new syscall8 v%i (libfs_patched %s)", test & 0xff, is_libfs_patched()? "found!": "not found");
        } else
        {       sys8_disable_all = 1;
                sprintf(payload_str, "payload-sk10 - new syscall8 Err?! v(%i)", test);
        }
    }

    // ADVERTENCIA: este codigo no debe ser removido de aqui para evitar pantalla negra por los parches (problemas de cache)

  
    tiny3d_Init(1024*1024);
    ioPadInit(7);
    usleep(250000);

   

    if(sys8_disable_all!=0) {
         if(DrawDialogYesNo2("Syscall 8 very old or not detected\n\nWant you REBOOT the PS3? (NO for XMB exit)")==1) 
             {set_install_pkg = 1;game_cfg.direct_boot=0; exit(0);}
         else exit(0);
    }

    if(must_patch) {
        sys8_pokeinstr(0x80000000007EF220ULL, 0x0ULL);
    }

    // disable ps1 emulation
    unload_psx_payload();

    {
    FILE *fp =fopen("/dev_hdd0/game/HOMELAUN1/path.bin", "rb");
    if(fp) {fclose(fp);homelaun= 1;fp =fopen("/dev_hdd0/game/HOMELAUN1/path2.bin", "rb");if(fp) {fclose(fp);homelaun= 2;}}
    }

    // turn off
    sys8_perm_mode(1);
    usleep(5000);


    // sys8_perm_mode(0);
    sys8_path_table(0LL);

   
    if(lv2_patch_storage) { // for PSX games
        if(lv2_patch_storage()<0) lv2_patch_storage = NULL;
    }


    if(payload_mode < ZERO_PAYLOAD) //if mode is wanin or worse, launch advert
    {
        DrawDialogOK(temp_buffer);
    }

   
    sprintf(temp_buffer, "%s/config", self_path);
    mkdir_secure(temp_buffer);
    
    sprintf(temp_buffer, "%s/cache", self_path);
    mkdir_secure(temp_buffer);
    
    sprintf(temp_buffer, "%s/self", self_path);
    mkdir_secure(temp_buffer);

    sprintf(temp_buffer, "%s/PKG", self_path);
    mkdir_secure(temp_buffer);
    
        
	// Load texture

    LoadTexture();

    init_twat();

    // initialize manager conf

    memset(&manager_cfg, 0, sizeof(manager_cfg));

    for(n=0; n<4; n++) 
        manager_cfg.videoscale_x[n] = 1024;

    manager_cfg.background_sel = 0;

    //load cfg and language strings
    LoadManagerCfg();

    noBDVD = manager_cfg.noBDVD;
    gui_mode = manager_cfg.gui_mode & 15;
    if(gui_mode == 1) sort_mode = (manager_cfg.gui_mode>>4); else sort_mode = 0;

/*
    // disable ps1 emulation
    if(noBDVD && lv2peek(0x8000000000001820ULL) == 0x455053315F454D55ULL) {
        sys8_pokeinstr(0x8000000000001830ULL, (u64)((1ULL<<32) | 1ULL)); // disable emulation
    }
*/
    // load ps3 disc less payload
    if(noBDVD == 2) {
        //lv2poke(syscall_base +(u64) (40 * 8), lv2peek(syscall_base));
        load_ps3_discless_payload();

        if(firmware == 0x341C || firmware == 0x355C)
            syscall_40(6, 0x8d000B04);
        else /*if(firmware == 0x421C)
            syscall_40(6, 0x8d001A04);
        else*/
            if(firmware != 0x430C && firmware != 0x431C && firmware != 0x440C && manager_cfg.event_flag)
                syscall_40(6, manager_cfg.event_flag);

    }

    load_ps3_controlfan_payload();

    load_controlfan_config();
    set_device_wakeup_mode(0);
    
    LoadPSXOptions(NULL);

    if(videoscale_x >= 1024) {
        videoscale_x = videoscale_y = 0;
        video_adjust();
    }

    if(manager_cfg.hdd_folder[0] == 0) {
        
        Select_games_folder();
     
        if(manager_cfg.hdd_folder[0] == 0) strcpy(manager_cfg.hdd_folder, __MKDEF_MANAGER_DIR__);
        SaveManagerCfg();
    } 
    
    strncpy(hdd_folder, manager_cfg.hdd_folder, 64);
   

    double sx = (double) Video_Resolution.width;
    double sy = (double) Video_Resolution.height;
    double psx = (double) (1000 + videoscale_x)/1000.0;
    double psy = (double) (1000 + videoscale_y)/1000.0;
    
    tiny3d_UserViewport(1, 
        (float) ((sx - sx * psx) / 2.0), // 2D position
        (float) ((sy - sy * psy) / 2.0), 
        (float) ((sx * psx) / 848.0),    // 2D scale
        (float) ((sy * psy) / 512.0),
        (float) ((sx / 1920.0) * psx),  // 3D scale
        (float) ((sy / 1080.0) * psy));

    select_px = select_py = 0;

    fdevices=0;
	fdevices_old=0;
	forcedevices=0;
	find_device=0;

    //syscall36("/dev_bdvd");
    add_sys8_bdvd(NULL, NULL);
    
    //sys8_perm_mode((u64) 0);

    unpatch_bdvdemu();

    if(noBDVD) {
        
        //sys_fs_umount("/dev_ps2disc");
        sys_fs_umount("/dev_bdvd");
        sleep(0);
        sys_fs_mount("CELL_FS_UTILITY:HDD1", "CELL_FS_FAT", "/dev_bdvd", 1);
        
    }


    init_music();
    
    if(!noBDVD && lv2_patch_storage) {
        if(!bdvd_notify) {
            DIR  *dir;
            dir = opendir("/dev_bdvd");

                if (dir)
                    closedir (dir);
                else {
                    if(firmware < 0x421C)
                        Reset1_BDVD();
                    else
                        Reset2_BDVD();
                }
        }

        bdvd_notify = 1;
    }

    
    sysDiscRegisterDiscChangeCallback(&DiscEjectCallback, &DiscInsertCallback);

    sprintf(temp_buffer, "%s/config/", self_path);

    LoadFavourites(temp_buffer, GAMEBASE_MODE);

    sprintf(temp_buffer, "%s/config/", self_path);

    LoadFavourites(temp_buffer, HOMEBREW_MODE);

    GetFavourites(mode_homebrew);

    sys_fs_umount("/dev_rewrite");

    if(lv2peek(0x80000000000004E8ULL) && noBDVD == 2) {
        syscall_40(1, 2);
    }

    if(noBDVD == 2 && syscall_40(3, 0) == 0) {
        DrawDialogTimer(language[PLUG_STORAGE1], 2000.0f);
    }

    // read xRegitry datas
    read_from_registry();

    if(sys_parental_level !=0 && sys_parental_level < 9) options_locked = 1;

    while(!exit_program) {

        float x = 0.0f, y = 0.0f;
    
        flash = (frame_count >> 5) & 1;

        frame_count++;

        int count_devices=0;

        int found_game_insert=0;
        
        int found_game_remove=0;

        if(tiny3d_MenuActive()) frame_count = 32; // to avoid the access to hdd when menu is active


        if(forcedevices || (frame_count & 63)==0 || fdevices == 0)
	    for(find_device = 0; find_device < 12; find_device++) {
            
			if(find_device==11) sprintf(filename, "/dev_bdvd");
			else if(find_device==0) sprintf(filename, "/dev_hdd0");
			else sprintf(filename, "/dev_usb00%c", 47+find_device);

           
            if((!forcedevices || (fdevices & 2048)) && find_device == 11 && bdvd_notify == 0) {goto skip_bdvd;}
          

            sysFSStat dstat;
            int ret;
            
            if(find_device == 11 && psx_inserted < 0) {
                ret = -1;
            } else
                ret= sysLv2FsStat(filename, &dstat);

			if (ret == 0) {

                // check bdemu
                if((fdevices & (1<<find_device)) == 0 && find_device >= 0 && find_device < 11) 
                    move_bdemubackup_to_origin(1 << find_device);

                fdevices|= 1<<find_device;

                // check psx
                if(find_device==11) {
             
                    if(!noBDVD && (get_psx_region_cd() & 0x10) == 0x10) {psx_inserted |= 0x100;}
                    else {

                        ret= sysLv2FsStat("/dev_bdvd/PS3_GAME", &dstat);
                        if(ret != 0) {psx_inserted = 0; fdevices^= 1<<find_device;}
                    }
                }

				
                if(find_device == 11 && get_disc_ready()) bdvd_notify = 0;
		    } else {
		        // check psx
                if(find_device==11 && !noBDVD && (get_psx_region_cd() & 0x10) == 0x10) 
                    {bdvd_notify = 0;fdevices|= 1<<find_device; psx_inserted |= 0x100;}
                else {
                    if(find_device == 11) psx_inserted = 0;
                    fdevices&= ~ (1<<find_device);
                }
            }

            skip_bdvd:
            

			// limit to 3 the devices selectables
			if(((fdevices>>find_device) & 1) && find_device!=11) {
			
                count_devices++;

				if(count_devices>3) fdevices&= ~ (1<<find_device);

			}

			// bdvd
			if(find_device==11) {
				
				if((fdevices!=fdevices_old || ((forcedevices>>find_device) & 1))) {
				
                    struct stat s;
                    found_game_insert = 1;
                    currentdir=0;

                    // detect psx code
                    if(!noBDVD && (stat("/dev_bdvd/PSX.EXE", &s)==0 || stat("/dev_bdvd/SYSTEM.CNF", &s)==0 || (psx_inserted & 0x100) == 0x100)) {
                        psx_inserted &= ~0x100;
                        psx_inserted |= 0x80;
                        strncpy(bluray_game, "PSX-GAME", 64);
                        bdvd_ejected = 0;
                        if(!noBDVD && lv2_patch_storage) {
                            psx_inserted|= get_psx_region_cd();
                            strncpy(bluray_game, (char *) psx_id, 64);
                            mode_favourites = 0;
                            select_option = 0;     
                            menu_screen = 0;
                            select_px = 0;
                            select_py = 0;
                        }

                    }
                    else {
           
					    sprintf(filename, "/dev_bdvd/PS3_GAME/PARAM.SFO");
					    bluray_game[0]=0;
					    // whatever... unused if -- remove later?
					    if(parse_param_sfo("/dev_bdvd/PS3_GAME/PARAM.SFO", bluray_game)==-1);
					    bluray_game[63]=0;
					}
					found_game_insert=1;		
					if(((fdevices>>11) & 1)  && !mode_homebrew && !noBDVD) {

						if(ndirectories>=MAX_DIRECTORIES) ndirectories= MAX_DIRECTORIES-1;
                        					
						sprintf(directories[ndirectories].path_name, "/dev_bdvd");
								
					    memcpy(directories[ndirectories].title, bluray_game, 63);
						directories[ndirectories].title[63]=0;
						directories[ndirectories].flags=(1<<11) | ((psx_inserted & 0xff)<<16);
                        if(!psx_inserted) {
                            sprintf(filename, "%s/%s", directories[ndirectories].path_name, "PS3_DISC.SFB" );
                            parse_ps3_disc((char *) filename, directories[ndirectories].title_id);
                            directories[ndirectories].title_id[63]=0;
                        } else
                            strncpy(directories[ndirectories].title_id, "PSX-GAME", 64);

                        ndirectories++;
                        found_game_insert=1;

                        s32 fdr;

                        if(!noBDVD && !sysLv2FsOpen("/dev_bdvd/PS3_GAME/USRDIR/EBOOT.BIN", 0, &fdr, S_IRWXU | S_IRWXG | S_IRWXO, NULL, 0)) {
                            u64 bytes;
                            u32 dat;
                            bdvd_ejected = 0;
                            static int counter = 0;
                            if(sysLv2FsRead(fdr, (void *) &dat, 4, &bytes)!=0) bytes =0LL;

                            if(bytes == 4 && dat != 0x53434500) {
                                if(counter==0) {
                                    counter = 1;
                                    if(!noBDVD && lv2_patch_storage) {
                                        bdvd_notify = 0;
                                        Eject_BDVD(EJECT_BDVD);
                                        Eject_BDVD(NOWAIT_BDVD | LOAD_BDVD);
          
                                    } else DrawDialogOK("Warning!. You must eject/load the disc");
                                } else DrawDialogOK("Warning!. Disc authentication failed");

                                ndirectories--;
                                
                                psx_inserted = 0;
                                fdevices&= ~ (1<<11);
                                goto skip_bdvd;
                            
                            } else {counter = 0;
                                if(!sys_ss_media_id(temp_buffer + 2048)) {
                                    struct stat s;
                                    sprintf(temp_buffer, "%s/config/%s.did", self_path, directories[ndirectories - 1].title_id);
                                    if(stat(temp_buffer, &s) != 0 || s.st_size != 0x10) 
                                        SaveFile(temp_buffer, (char *) temp_buffer + 2048, 0x10);
                                }
                            }

                        }

                        //stops_BDVD = 0;
                        //Eject_BDVD(NOWAIT_BDVD | STOP_BDVD);
                        //parse_param_sfo_id(filename, directories[ndirectories].title_id);
                        //directories[ndirectories].title_id[63]=0;
                       
						

		    	    } else {
						
                        delete_entries(directories, &ndirectories, (1<<11));
                        found_game_remove=1;
                    }

					sort_entries2(directories, &ndirectories, sort_mode);              
				}
				
				forcedevices &= ~ (1<<find_device);
				fdevices_old &= ~ (1<<find_device);
				fdevices_old |= fdevices & (1<<find_device);
			} else
			// refresh list 
			if(fdevices!=fdevices_old || ((forcedevices>>find_device) & 1)) {
					
                currentdir=0;
				found_game_insert = 1;	
				forcedevices &= ~ (1<<find_device);

				if(find_device==0) {
                    if (!memcmp(hdd_folder,"dev_hdd0",9)) {
                        sprintf(filename, "/%s/" __MKDEF_GAMES_DIR,hdd_folder); 
                    } else if (!memcmp(hdd_folder,"dev_hdd0_2", 11)) {
                        sprintf(filename, "/%s/GAMES", "dev_hdd0"); 
                    } 
                    else {
                        sprintf(filename, "/dev_hdd0/game/%s/" __MKDEF_GAMES_DIR,hdd_folder);  
                    }
                   
                    if(mode_homebrew)
                        sprintf(filename, "/dev_hdd0/game");
                    
 
                    sysFsGetFreeSize("/dev_hdd0/", &blockSize, &freeSize);
                    freeSpace[find_device] = ( ((u64)blockSize * freeSize));
                    freeSpace[find_device] = freeSpace[find_device] / 1073741824.0;
            
                } else {
                    sprintf(filename, "/dev_usb00%c/", 47+find_device);
                    sysFsGetFreeSize(filename, &blockSize, &freeSize);
                    double space = ( ((double)blockSize) * ((double) freeSize) ) /  1073741824.0;
                    freeSpace[find_device] = (float) space;
                    if(!mode_homebrew)
                        sprintf(filename, "/dev_usb00%c/" __MKDEF_GAMES_DIR, 47+find_device);
                    else
                        sprintf(filename, "/dev_usb00%c/game", 47+find_device);
                    
                }

		        if(((fdevices>>find_device) & 1) && (!mode_homebrew || (mode_homebrew && find_device!=0)) ) {
				    fill_entries_from_device(filename, directories, &ndirectories, (1<<find_device) | ((1<<31) * (mode_homebrew!=0)), 0 | (2 * (mode_homebrew!=0)));
                    found_game_insert=1;
                } else {
					
                    delete_entries(directories, &ndirectories, (1<<find_device));
                    found_game_remove=1;
                }
				
                sort_entries2(directories, &ndirectories, sort_mode);

				fdevices_old&= ~ (1<<find_device);
				fdevices_old|= fdevices & (1<<find_device);
			}
		}
       
        if (found_game_insert || found_game_remove){

          UpdateFavourites(directories, ndirectories);

          if(mode_favourites && !havefavourites) mode_favourites = 0;
          get_games();
          load_gamecfg(-1); // force refresh game info

          mode_favourites = mode_favourites != 0; // avoid insert favourites

          select_option = 0;     
          menu_screen = 0;
        }
     
        found_game_remove=0;
        found_game_insert=0;

        pause_music(0);

        if(0)
        if(stops_BDVD) {
            stops_BDVD++;
            if(stops_BDVD >= 60 * 2) {
                stops_BDVD = 0;
                Eject_BDVD(NOWAIT_BDVD | STOP_BDVD);
            }
        }

        /////////////////////////////////////

        if(gui_mode == 1)
            cls0();
        else {
            cls();

            update_twat();
        }


        x= (848 - 640) / 2; y=(512 - 360) / 2;
//        DrawBox(x - 16, y - 16, 65535.0f, 640.0f + 32, 360 + 32, 0x00000028);
  //      DrawBox(x, y, 65535.0f, 640.0f, 360, 0x30003018);

         
        x= 28; y= 0;

        if((old_pad & (BUTTON_L2 | BUTTON_R2 | BUTTON_START)) == (BUTTON_L2 | BUTTON_R2 | BUTTON_START)) {
            
            videoscale_x = videoscale_y = 0;
            video_adjust();
        }

        // paranoid checks

        if(select_px < 0 || select_px > 3) select_px = 0;
        if(select_py < 0 || select_py > 2) select_py = 0;
        if(currentdir < 0 || currentdir >= ndirectories) currentdir = 0;
        if(currentgamedir < 0 || currentgamedir >= ndirectories) currentgamedir = 0;

       
       // paranoid favourite check
        for(n = 0; n < 12; n++) {
            if(favourites.list[n].index >=0) {
                if(favourites.list[n].title_id[0] == 0) exit(0);
                if(favourites.list[n].index >= ndirectories) exit(0);
                if(directories[favourites.list[n].index].flags == 0) exit(0);
            }
        }
        
        // fake disc insertion

        if(noBDVD == 2 && disc_less_on == 1  && syscall_40(3, 0) == 1) {
            int u;
            
            s16 * sound = memalign(32, (cricket_raw_bin_size + 31) & ~31);
            
            if(sound) {
                memset((void *) sound, 0, (cricket_raw_bin_size + 31) & ~31);
                memcpy((void *) sound, (void *)  cricket_raw_bin + 1, cricket_raw_bin_size - 1);
            
                SND_SetVoice(1, VOICE_MONO_16BIT, 22050, 0, sound, cricket_raw_bin_size - 1, 0x40, 0x20, NULL);
                SND_SetVoice(1, VOICE_MONO_16BIT, 22050, 1000, sound, cricket_raw_bin_size - 1, 0x20, 0x40, NULL);
            }

            for(u = 0; u <190; u++)  {
                cls();
              
                tiny3d_SetTextureWrap(0, Png_res_offset[0], Png_res[0].width, 
                    Png_res[0].height, Png_res[0].wpitch, 
                    TINY3D_TEX_FORMAT_A8R8G8B8,  TEXTWRAP_CLAMP, TEXTWRAP_CLAMP,1);
                update_twat();
                DrawTextBox((848 - 300)/2,(512 - 300)/2, 0, 300, 300, ((u>=64) ? 0xff : u<<2) | 0xffffff00);
                SetCurrentFont(FONT_TTF);
                SetFontColor(((u & 16)) ? 0x0 : 0xffffffff, 0x00000000);

                SetFontSize(32, 48);

                SetFontAutoCenter(1);
                DrawString(0, 512 - 75,  language[PLUG_STORAGE2]);
                SetFontAutoCenter(0);
                tiny3d_Flip();

                ps3pad_read();
            }

            if(sound) free(sound);

            u32 eid= (u32) syscall_40(4, 0);

            if(eid!=0) {
                eid -= 0x100;
                manager_cfg.event_flag = eid;
                SaveManagerCfg();
            }

            cls();
            update_twat();
            disc_less_on = 2;

            if(new_pad & BUTTON_CROSS) new_pad ^= BUTTON_CROSS;
            if(new_pad & BUTTON_CIRCLE) new_pad ^= BUTTON_CIRCLE;

            
        }
            
        switch(menu_screen) {
            case 0:
                if(gui_mode == 0) draw_gui1(x, y);
                else draw_gui2(x, y);
                gui_control();
                break;
            case 1:
                draw_options(x, y, currentgamedir);
                break;
            case 2:
                draw_configs(x, y, currentgamedir);
                break;
            case 3:
                draw_gbloptions(x, y);
                break;
            case 4:
                draw_toolsoptions(x, y);
                break;
            case 5:
                draw_cachesel(x, y);
                break;
            case 444:
                draw_psx_options(x, y, currentgamedir);
                break;
            case 445:
                draw_psx_options2(x, y, currentgamedir);
                break;
            default:
                menu_screen = 0;
                break;
        }

        auto_ftp(); // auto enable the ftp
     }
     
/// Patch error 0x80010009
	if( firmware == 0x446C ) return 0; // patch not required
	
	char id[ 10 ];
	int ret_g, ret_u;
	
	pause_music(1);
	
	if( _parse_param_sfo_id( "/app_home/PS3_GAME/PARAM.SFO", id ) != 0 ) return 0; //Game not mounted
	
// Verified Update
	char path_update[ 43 ];
	char path_update_eboot[ 43 ];
	sysFSStat status;
	
	snprintf( path_update_eboot, 43, "/dev_hdd0/game/%s/USRDIR/EBOOT.BIN", id );
	ret_u = compare_fw_ver( path_update_eboot );
	if( sysLv2FsStat( "/app_home/PS3_GAME/fixed09", &status ) == 0 && ret_u == 0 ) return 0; //Nothing to patch

// Verified Game	
	ret_g = compare_fw_ver( "/app_home/PS3_GAME/USRDIR/EBOOT.BIN" );
	if( ret_g == 0 && ret_u == 0 ) return 0; //Nothing to patch

//Patch game or update
	int fd;

	if( ret_g == -1 && ( ret_u == 0 || ret_u == -4 ) ) // Patch game
	{
		DrawDialogOK( language[PATCH09_WAITGAME] );
		patch_error_09( "/app_home/PS3_GAME/USRDIR" );
		sysLv2FsOpen( "/app_home/PS3_GAME/fixed09", SYS_O_WRONLY | SYS_O_CREAT | SYS_O_TRUNC, &fd, 0, NULL, 0 );
		sysLv2FsClose( fd );
	}
	
	else if( ret_u == -1 && ( ret_g == 0 || ret_g == -1 ) ) // Patch update
	{
		DrawDialogOK( language[PATCH09_WAITUPDATE] );
		snprintf( path_update, 43, "/dev_hdd0/game/%s/USRDIR", id );
		patch_error_09( path_update ); 
	}

	else if( ret_g == -2 && ret_u == -4  ) //Rebuild game, update not exist
		DrawDialogOK( language[PATCH09_REBUILDGAME] );
	
	else if( ret_u == -2 && ( ret_g == 0 || ret_g == -2 ) ) //Delete update or rebuild it
		DrawDialogOK( language[PATCH09_REBUILDUPDATE] );
	
	else if( ret_u == -2 && ret_g == -2 ) // Update and game not bootable
		DrawDialogOK( language[PATCH09_REBUILDUPDATE] );
	
	return 0;
}

// draw_cachesel
struct {
    u64 size;
    char title[64];
    char title_id[64];
} cache_list[64];

int ncache_list = 0;

void LoadCacheDatas() 
{
    DIR  *dir, *dir2;

    sprintf(temp_buffer, "%s/cache", self_path);
    dir = opendir (temp_buffer);
    if(!dir) return;

    sysFsGetFreeSize("/dev_hdd0/", &blockSize, &freeSize);
    freeSpace[0] = ( ((u64)blockSize * freeSize));
    freeSpace[0] = freeSpace[0] / 1073741824.0;

    ncache_list = 0;

    while(1) {
        struct dirent *entry= readdir (dir);
        
        if(!entry) break;
        if(entry->d_name[0]=='.') continue;

        if(!(entry->d_type & DT_DIR)) continue;

        strncpy(cache_list[ncache_list].title_id, entry->d_name, 64);

        cache_list[ncache_list].size = 0ULL;

        sprintf(temp_buffer + 1024, "%s/cache/%s/name_entry", self_path, entry->d_name);
        int size;
        char *name = LoadFile(temp_buffer + 1024, &size);

        memset(cache_list[ncache_list].title, 0, 64);
        if(name) {
            memcpy(cache_list[ncache_list].title, name, (size < 64) ? size : 63);
            free(name);
        }

        sprintf(temp_buffer + 1024, "%s/cache/%s", self_path, entry->d_name);
        dir2 = opendir (temp_buffer + 1024);
        if(dir2) {
            while(1) {
                struct dirent *entry2= readdir (dir2);
                struct stat s;
                
                if(!entry2) break;
                if(entry2->d_name[0]=='.') continue;

                if((entry2->d_type & DT_DIR)) continue;

                sprintf(temp_buffer + 2048, "%s/cache/%s/%s", self_path, entry->d_name, entry2->d_name);
                if(stat(temp_buffer + 2048, &s) == 0) {
                    cache_list[ncache_list].size += s.st_size;
                }
            }
        }

        ncache_list++; if(ncache_list >= 64) break;
    }
}

inline int get_currentdir(int i)
{
    if(mode_favourites !=0)
        return favourites.list[i].index;

    return (currentdir + i);
}

inline int get_int_currentdir(int i)
{
    if(mode_favourites !=0)
        return favourites.list[i].index;

    return (int_currentdir + i);
}

void load_gamecfg (int current_dir)
{

    static int last_selected = -1;
    
    if(current_dir < 0) //check reset info
    {
        last_selected = current_dir;
        memset(&game_cfg, 0, sizeof(game_cfg));
        return;
    }
    
    if(last_selected == current_dir)
        return;
    
    last_selected = current_dir; //prevents load again
    
    sprintf(temp_buffer, "%s/config/%s.cfg", self_path, directories[current_dir].title_id);
    memset(&game_cfg, 0, sizeof(game_cfg));

    int file_size;
    char *file = LoadFile(temp_buffer, &file_size);
    if(file)
    {
        if(file_size > sizeof(game_cfg)) file_size = sizeof(game_cfg);
        memcpy(&game_cfg, file, file_size);
        free(file);
    }
}

int check_disc(void)
{
    int get_user = 1;

    while(get_user == 1)
    {
        DIR  *dir;
        dir = opendir ("/dev_bdvd");

        if(dir)
        {
            closedir (dir);
            return 1;
        }
        else
            get_user = DrawDialogYesNo(language[DRAWSCREEN_REQBR]);
    }
    
    return -1;
}

void mount_custom(char *path)
{
    int size, n;
    char * mem;
    sprintf(temp_buffer, "%s/ps2disc.txt", path);
    
    mem= LoadFile(temp_buffer, &size);

    if(mem) {
        n=0; while(mem[n]>=32) n++;

        mem[n]=0;
        add_sys8_path_table("/dev_ps2disc", mem);
        free(mem);
    }
}

static u8 BdId[0x10];

void set_BdId(int index)
{
    struct stat s;
    int readed;
    int ret = 0;
    void *mem;

    if(!lv2peek(0x80000000000004E8ULL)) return;
    
    sprintf(temp_buffer, "%s/config/%s.did", self_path, directories[index].title_id);
    sprintf(temp_buffer + 1024, "%s/BDMEDIA_ID", directories[index].path_name);
    
    memset(BdId, 0, 0x10);
    
    ret = -1;

    if(stat(temp_buffer + 1024, &s) == 0 && s.st_size == 0x10) {
        mem = LoadFile((void *) temp_buffer + 1024, &readed);
        if(!mem || readed!=0x10) {if(mem) free(mem); ret = -1;}
        else {ret = 0; memcpy(BdId, mem, 0x10); free(mem);}
    }

    if(ret!=0 && stat(temp_buffer, &s) == 0 && s.st_size == 0x10) {
        mem = LoadFile((void *) temp_buffer, &readed);
        if(!mem || readed!=0x10) {if(mem) free(mem); ret = -1;}
        else {ret = 0; memcpy(BdId, mem, 0x10); 
             SaveFile(temp_buffer + 1024, (char *) BdId, 0x10);
             free(mem);
        }
    }
 
    syscall_40(7, (u64) (BdId)); // set BD Media Id

}

void get_pict(int *index)
{
    char dir[0x420];
    char dir2[0x420];
    char temp_buffer[0x420];

    sysFSStat dstat;

    int i = *index;

    
    if(!mode_favourites || (mode_favourites != 0 && favourites.list[i].index >= 0)) {
        sprintf(dir, "%s%s", directories[get_int_currentdir(i)].path_name, 
            &folder_mode[!((directories[get_int_currentdir(i)].flags>>31) & 1)][0]);

        sprintf(dir2, "%s%s", directories[(mode_favourites != 0) 
                ? favourites.list[i].index : (int_currentdir + i)].path_name, 
                &folder_mode[!((directories[(mode_favourites != 0) 
                    ? favourites.list[i].index : (int_currentdir + i)].flags>>31) & 1)][0]);

        sprintf(temp_buffer, "%sPIC1.PNG", dir);
       
        if(sysLv2FsStat(temp_buffer, &dstat) != 0 || LoadTexturePNG(temp_buffer, 12) < 0) {
            sprintf(temp_buffer, "%sPIC0.PNG", dir2);

            if(sysLv2FsStat(temp_buffer, &dstat) != 0 || LoadTexturePNG(temp_buffer, 12) < 0) {
                sprintf(temp_buffer, "%sICON0.PNG", dir2);
                if(sysLv2FsStat(temp_buffer, &dstat) != 0 || LoadTexturePNG(temp_buffer, 12) < 0) {
                    sprintf(temp_buffer, "%sPIC2.PNG", dir2);
                    if(sysLv2FsStat(temp_buffer, &dstat) != 0 || LoadTexturePNG(temp_buffer, 12) < 0) Png_offset[12] = 0;
                }
            }
        }
    }
}

void draw_gui1(float x, float y)
{
    int i, n, m;

    float x2;

    //static char str_home[2][16]={" Homebrew",""};
    static char str_home[4][16]={" Homebrew", "", " PS3", " PSX"};

    int str_type = (mode_homebrew == HOMEBREW_MODE) ? 0 : 1 + game_list_category;

    int selected = select_px + select_py * 4;

    SetCurrentFont(FONT_TTF);

    // header title

    DrawBox(x, y, 0, 200 * 4 - 8, 18, 0x00000028);

    SetFontColor(0xffffffff, 0x00000000);

    SetFontSize(18, 20);

    SetFontAutoCenter(0);

    if(mode_favourites >= 131072) {
        DrawFormatString(x, y, " %s%s", language[DRAWSCREEN_FAVSWAP], &str_home[str_type][0]);
    }
    else if(mode_favourites >= 65536) {
        DrawFormatString(x, y, " %s%s", language[DRAWSCREEN_FAVINSERT], &str_home[str_type][0]);
    }
    else if(mode_favourites) {
        DrawFormatString(x, y, " %s%s", language[DRAWSCREEN_FAVORITES], &str_home[str_type][0]);
    }
    else {
        DrawFormatString(x, y, " %s %i/%i (%i %s)%s", language[DRAWSCREEN_PAGE], currentdir/12 + 1, ROUND_UP12(ndirectories)/12, ndirectories, language[DRAWSCREEN_GAMES], &str_home[str_type][0]);
    }

    // list device space

    m = selected;

    if(Png_offset[m]) {

        i = -1;

        if(!mode_favourites || ((mode_favourites !=0) && favourites.list[m].index >= 0))
            for(i = 0; i < 11; i++)
                if((directories[(mode_favourites !=0) ? favourites.list[m].index : (currentdir + m)].flags & GAMELIST_FILTER)== (1<<i)) break;
        m = i;
    } else m = -1;

    x2 = 1200;
    for(n = 0; n < 2; n++) {
        for(i = 0; i < 11; i++) {

            if(((fdevices>>i) & 1)) {
                
                if(m == i) SetFontColor(0xafd836ff, 0x00000000); else SetFontColor(0xffffff44, 0x00000000);
                if(i==0)
                    x2= DrawFormatString(x2, 0, "hdd0: %.2fGB ", freeSpace[i]);
                else
                    x2= DrawFormatString(x2, 0, "usb00%c: %.2fGB ", 47 + i, freeSpace[i]);
            }

        }
    
        x2 = 848 -(x2 - 1200) - x; 
    }

    SetFontAutoCenter(0);
    SetFontSize(18, 20);

    SetFontColor(0xffffffff, 0x00000000);

    y += 24;
    
    i = 0;
    int flash2 = 0;
    #define MAX_FLASH 32
    if (frame_count & MAX_FLASH) flash2= (MAX_FLASH-1) - (frame_count & (MAX_FLASH-1)); else flash2= (frame_count & (MAX_FLASH-1));
    for(n = 0; n < 3; n++) 
        for(m = 0; m < 4; m++) {
            int f = (select_px == m && select_py == n);
            float f2 = (int) f;
            
            if(background_sel > 3) f2 = 3* ((float) (flash2 *(select_px == m && select_py == n)))/((float) MAX_FLASH);
            
            DrawBox(x + 200 * m - 4 * f2, y + n * 150 - 4 * f2, 0, 192 + 8 * f2, 142 + 8 * f2, 0x00000028 + (flash2 * f) );

            //draw Splited box
            //if(directories[currentgamedir].splitted)
            //    DrawBox(x + 198 * m, (y - 2) + n * 150, 0, 194, 144, 0x55ff3328 );
            
            if(Png_offset[i]) {
               
                if((directories[get_currentdir(i)].flags  & ((1<<11) | (1<<23)))== ((1<<11) | (1<<23))) {
                    Png_iscover[i] = -1;
                    tiny3d_SetTextureWrap(0, Png_res_offset[5], Png_res[5].width, 
                                             Png_res[5].height, Png_res[5].wpitch, 
                                             TINY3D_TEX_FORMAT_A8R8G8B8,  TEXTWRAP_CLAMP, TEXTWRAP_CLAMP,1);
                } else if(Png_iscover[i] < 1 && (directories[get_currentdir(i)].flags  & (1<<23))) { // add PSX iso
                    Png_iscover[i] = -1;
                    tiny3d_SetTextureWrap(0, Png_res_offset[6], Png_res[6].width, 
                                             Png_res[6].height, Png_res[6].wpitch, 
                                             TINY3D_TEX_FORMAT_A8R8G8B8,  TEXTWRAP_CLAMP, TEXTWRAP_CLAMP,1);
                } else  
                    tiny3d_SetTextureWrap(0, Png_offset[i], Png_datas[i].width, 
                                             Png_datas[i].height, Png_datas[i].wpitch, 
                                             TINY3D_TEX_FORMAT_A8R8G8B8,  TEXTWRAP_CLAMP, TEXTWRAP_CLAMP,1);

                if(Png_iscover[i] == -1)
                    DrawTextBox(x + 25 + 200 * m - 4 * f2, y + n * 150 - 4 * f2, 0, 142 + 8 * f2, 142 + 8 * f2, (0xffffffff- (MAX_FLASH * 2 * f)) + (flash2 * 2 * f));
                else if(Png_iscover[i] == 1)
                    DrawTextBox(x + 36 + 200 * m - 4 * f2, y + n * 150 - 4 * f2, 0, 124 + 8 * f2, 142 + 8 * f2, (0xffffffff- (MAX_FLASH * 2 * f)) + (flash2 * 2 * f));
                else
                    DrawTextBox(x + 200 * m - 4 * f2, y + n * 150 - 4 * f2, 0, 192 + 8 * f2, 142 + 8 * f2, (0xffffffff- (MAX_FLASH * 2 * f)) + (flash2 * 2 * f));
                
               // if((mode_favourites !=0) && favourites.list[i].index < 0) exit(0);
                if((mode_favourites !=0) && favourites.list[i].index < 0) exit(0);

                if(!mode_favourites || ((mode_favourites !=0) && favourites.list[i].index >= 0)) {
                    // ignore bluray icon
                    if((directories[get_currentdir(i)].flags  & ((1<<11) | (1<<23)))== ((1<<11) | (1<<23))) ;
                    // draw Bluray icon
                    else if((directories[get_currentdir(i)].flags  & GAMELIST_FILTER)== (1<<11)) {
                        tiny3d_SetTextureWrap(0, Png_res_offset[0], Png_res[0].width, 
                                                 Png_res[0].height, Png_res[0].wpitch, 
                                                 TINY3D_TEX_FORMAT_A8R8G8B8,  TEXTWRAP_CLAMP, TEXTWRAP_CLAMP,1);
                        DrawTextBox(x + 200 * m + 4 - 4 * f2, y + n * 150 + 4 - 4 * f2, 0, 32, 32, 0xffffffcf);
                    } else 
                    // draw Usb icon    
                    if((directories[get_currentdir(i)].flags  & GAMELIST_FILTER)> 1) {
                        tiny3d_SetTextureWrap(0, Png_res_offset[1], Png_res[1].width, 
                        Png_res[1].height, Png_res[1].wpitch, 
                            TINY3D_TEX_FORMAT_A8R8G8B8,  TEXTWRAP_CLAMP, TEXTWRAP_CLAMP,1);

                        if(directories[get_currentdir(i)].splitted)
                            DrawTextBox(x + 200 * m + 4 - 4 * f2, y + n * 150 + 4 - 4 * f2, 0, 32, 24, 0xff9999aa);
                        else
                            DrawTextBox(x + 200 * m + 4 - 4 * f2, y + n * 150 + 4 - 4 * f2, 0, 32, 24, 0xffffffcf);
                        
                    }
                }

            } else if(mode_favourites && favourites.list[i].title_id[0] != 0) {
                tiny3d_SetTextureWrap(0, Png_res_offset[2], Png_res[2].width, 
                    Png_res[2].height, Png_res[2].wpitch, 
                        TINY3D_TEX_FORMAT_A8R8G8B8,  TEXTWRAP_CLAMP, TEXTWRAP_CLAMP,1);
                    DrawTextBox(x + 200 * m + 32 - 4 * f2, y + n * 150 + 7 - 4 * f2, 0, 128 + 8 * f2, 128 + 8 * f2, 0xffffff3f);
            }
            
        i++;   
        }

    i = selected;

    if(flash) {

        int png_on = 0;

        //DrawBox(x + 200 * select_px - 4, y + select_py * 150 - 4 , 0, 200, 150, 0xa0a06080);
            

        if(mode_favourites >= 65536) {

            if(mode_favourites < 131072) {

                if(Png_offset[12]) {
                    tiny3d_SetTextureWrap(0, Png_offset[12], Png_datas[12].width, 
                        Png_datas[12].height, Png_datas[12].wpitch, 
                            TINY3D_TEX_FORMAT_A8R8G8B8,  TEXTWRAP_CLAMP, TEXTWRAP_CLAMP,1);
                    png_on = 1;
                }
            }
            else {
                i = mode_favourites - 131072;

                if(i>= 0 && i < 12) {
                    if(!Png_offset[i] && favourites.list[i].title_id[0] != 0) {
                        tiny3d_SetTextureWrap(0, Png_res_offset[2], Png_res[2].width, 
                            Png_res[2].height, Png_res[2].wpitch, 
                                TINY3D_TEX_FORMAT_A8R8G8B8,  TEXTWRAP_CLAMP, TEXTWRAP_CLAMP,1);
                        png_on = 1;
                            
                    } else if(Png_offset[i]){
                        png_on = 1;
                        tiny3d_SetTextureWrap(0, Png_offset[i], Png_datas[i].width, 
                        Png_datas[i].height, Png_datas[i].wpitch, 
                            TINY3D_TEX_FORMAT_A8R8G8B8,  TEXTWRAP_CLAMP, TEXTWRAP_CLAMP,1);
                    }
                }
            }

            if(png_on)
                DrawTextBox(x + 200 * select_px - 4, y + select_py * 150 - 4 , 0, 200, 150, 0x8fff8fcf);
        }
    
    }

    SetFontColor(0xffffffff, 0x00000000);

    // display temp
    if((frame_count & 0x100)) {
        static u32 temp = 0;
        static u32 temp2 = 0;
        int y2;

        if(temp==0 || (frame_count & 0x1f) == 0x0 ) {
            sys_game_get_temperature(0, &temp);
            sys_game_get_temperature(1, &temp2);
        }

        SetCurrentFont(FONT_TTF);
        SetFontSize(20, 20);
        
        x2= DrawFormatString(1024, 0, " Temp CPU: 99ºC RSX: 99ºC ");

        y2= y + 3 * 150 - 4 + 12;
        SetFontColor(0xffffffff, 0x00000000);
        x2= DrawFormatString(x + 4 * 200 - (x2 - 1024) - 12 , y2, " Temp CPU: ");
        if(temp < 80) SetFontColor(0xfff000ff, 0x00000000); else SetFontColor(0xff0000ff, 0x00000000);
        x2= DrawFormatString(x2, y2, "%uºC",  temp);
        SetFontColor(0xffffffff, 0x00000000);
        x2= DrawFormatString(x2, y2, " RSX: ");
        if(temp2 < 75) SetFontColor(0xfff000ff, 0x00000000); else SetFontColor(0xff0000ff, 0x00000000);
        x2= DrawFormatString(x2, y2, "%uºC ", temp2);

        SetFontColor(0xffffffff, 0x00000000);
    }
    else if(Png_offset[i])
    {
        SetCurrentFont(FONT_TTF);
        SetFontSize(20, 20);
        x2= DrawFormatString(1024, 0, " %s ", language[DRAWSCREEN_SOPTIONS]);
        DrawFormatString(x + 4 * 200 - (x2 - 1024) - 12 , y + 3 * 150 - 2, " %s ", language[DRAWSCREEN_SOPTIONS]);
   
    }
    else if(mode_favourites && mode_favourites < 65536 && favourites.list[i].title_id[0] != 0) 
    {
        SetCurrentFont(FONT_TTF);
        SetFontSize(20, 20);
        x2= DrawFormatString(1024, 0, " %s ", language[DRAWSCREEN_SDELETE]);
        DrawFormatString(x + 4 * 200 - (x2 - 1024) - 12 , y + 3 * 150 - 2, " %s ", language[DRAWSCREEN_SDELETE]);
    }
    else
    {
        DrawBox(x + 200 * select_px , y + select_py * 150 , 0, 192, 142, 0x404040a0);
        SetCurrentFont(FONT_TTF); // get default
        SetFontSize(20, 20);
    }
    
    if(!(frame_count & 0x100)) {
        x2= DrawFormatString(1024, 0, " %s ", language[DRAWSCREEN_STGLOPT]);
        DrawFormatString(x + 4 * 200 - (x2 - 1024) - 12 , y + 3 * 150 + 18, " %s ", language[DRAWSCREEN_STGLOPT]);
    }
    
    if((Png_offset[i])||(mode_favourites && mode_favourites < 65536 && favourites.list[i].title_id[0] != 0)) {
        DrawBox(x + 200 * select_px - 8 + (200 - 24 * 8)/2, y + select_py * 150 - 4 + 150 - 40, 0, 200, 40, 0x404040a0);
        SetCurrentFont(FONT_TTF);
        SetFontSize(24, 24);
        x2 = DrawFormatString(x + 200 * select_px - 4 + (200 - 24 * 8)/2, y + select_py * 150 - 4 + 150 - 40, "  %s", language[DRAWSCREEN_PLAY]);
    }

    // draw game name
    i = selected;

    DrawBox(x, y + 3 * 150, 0, 200 * 4 - 8, 40, 0x00000028);

    SetFontColor(0xffffffee, 0x00000000);

    if((Png_offset[i] && !mode_favourites) || (mode_favourites && favourites.list[i].title_id[0] != 0)) {

        
        u32 str_color = 0xffffffee;

        if(mode_favourites) {

            if(strncmp((char *) string_title_utf8, favourites.list[i].title, 64)) {
                strncpy((char *) string_title_utf8, favourites.list[i].title, 128);
                update_title_utf8 = 1;
            }

        } else if((directories[(currentdir + i)].flags  & GAMELIST_FILTER)== (1<<11)) {
         
            str_color = 0xafd836ee;
            if(strncmp((char *) string_title_utf8, bluray_game, 64)) {
                strncpy((char *) string_title_utf8, bluray_game, 128);
                update_title_utf8 = 1;
            }

        } else {

            if(strncmp((char *) string_title_utf8, directories[(currentdir + i)].title, 64)) {
                strncpy((char *) string_title_utf8, directories[(currentdir + i)].title, 128);
                update_title_utf8 = 1;
            }
        }

        if(update_title_utf8) {
            width_title_utf8 = Render_String_UTF8(ttf_texture, 768, 32, string_title_utf8, 16, 24);
            update_title_utf8 = 0;
        }

        tiny3d_SetTextureWrap(0, tiny3d_TextureOffset(ttf_texture), 768, 
                32, 768 * 2, 
                TINY3D_TEX_FORMAT_A4R4G4B4,  TEXTWRAP_CLAMP, TEXTWRAP_CLAMP,1);
        DrawTextBox(x + 3, y + 3 * 150 , 0, 768, 32, str_color);
       

        SetFontAutoCenter(0);

        load_gamecfg (get_currentdir(i)); // refresh game info

        if((game_cfg.useBDVD) || (game_cfg.direct_boot == 2))
        {
            tiny3d_SetTextureWrap(0, Png_res_offset[0], Png_res[0].width, 
                Png_res[0].height, Png_res[0].wpitch, 
                TINY3D_TEX_FORMAT_A8R8G8B8,  TEXTWRAP_CLAMP, TEXTWRAP_CLAMP,1);

            DrawTextBox(x + 200 * select_px + 148 + (200 - 23 * 8)/2, y + select_py * 150 - 4 + 150 - 36, 0, 32, 32, 0xffffff99);
        }
        
        if(game_cfg.direct_boot)
        {
            tiny3d_SetTextureWrap(0, Png_res_offset[3], Png_res[3].width, 
                Png_res[3].height, Png_res[3].wpitch, 
                TINY3D_TEX_FORMAT_A8R8G8B8,  TEXTWRAP_CLAMP, TEXTWRAP_CLAMP,1);

            DrawTextBox(x + 200 * select_px + 148 + (200 - 23 * 8)/2, y + select_py * 150 - 4 + 150 - 36, 0, 32, 32, 0xffffffff);        
        }

    }

    if(flash && ftp_inited) {
        tiny3d_SetTextureWrap(0, Png_res_offset[4], Png_res[4].width, 
            Png_res[4].height, Png_res[4].wpitch, 
            TINY3D_TEX_FORMAT_A8R8G8B8,  TEXTWRAP_CLAMP, TEXTWRAP_CLAMP,1);

        DrawTextBox(200 * 4 -32, y - 32, 0, 64, 32, 0xffffffff); 
    }

    //SetCurrentFont(FONT_DEFAULT);

    tiny3d_Flip();


}

void draw_gui2(float x, float y)
{
    int i, n, m;

    float x2;

    static char str_home[4][16]={" Homebrew", "", " PS3", " PSX"};

    int str_type = (mode_homebrew == HOMEBREW_MODE) ? 0 : 1 + game_list_category;

    int selected = select_px + select_py * 4;

    SetCurrentFont(FONT_TTF);

    // header title

    DrawBox(x, y, 0, 200 * 4 - 8, 18, 0x00000028);

    SetFontColor(0xffffffff, 0x00000000);

    SetFontSize(18, 20);

    SetFontAutoCenter(0);

    if(mode_favourites >= 131072) {
        DrawFormatString(x, y, " %s%s", language[DRAWSCREEN_FAVSWAP], &str_home[str_type][0]);
    }
    else if(mode_favourites >= 65536) {
        DrawFormatString(x, y, " %s%s", language[DRAWSCREEN_FAVINSERT], &str_home[str_type][0]);
    }
    else if(mode_favourites) {
        DrawFormatString(x, y, " %s%s", language[DRAWSCREEN_FAVORITES], &str_home[str_type][0]);
    }
    else {
     
        DrawFormatString(x, y, " %i/%i %s%s", 
             ndirectories ? select_px + currentdir + 1 : 0, ndirectories, language[DRAWSCREEN_GAMES], &str_home[str_type][0]);
    
    }

    u32 hh = 0, mm = 0, ss = 0, day = 0, month = 0, year = 0;

    PS3GetDateTime(&hh, &mm, &ss, &day, &month, &year);
    

    SetFontColor(0xffffffff, 0x00000000);
    SetCurrentFont(FONT_BUTTON);

    SetFontSize(8, 20);

    SetFontAutoCenter(0);

    float y2 = y + 80;

    y2= y + 3 * 150 - 4 + 12 + 24;
    x2 = x;

    if(sys_dateformat == 0)
        DrawFormatString(x2, y2, " %s, %04u/%02u/%02u %02u:%02u:%02u ", PS3TimeZone[sys_timezone % 110].name, year, month, day, hh, mm, ss);
    else if(sys_dateformat == 2)
        DrawFormatString(x2, y2, " %s, %02u/%02u/%04u %02u:%02u:%02u ", PS3TimeZone[sys_timezone % 110].name, month, day, year, hh, mm, ss);
    else
        DrawFormatString(x2, y2, " %s, %02u/%02u/%04u %02u:%02u:%02u ", PS3TimeZone[sys_timezone % 110].name, day, month, year, hh, mm, ss);

    SetFontAutoCenter(0);

    SetFontColor(0xffffffff, 0x00000000);

    SetCurrentFont(FONT_TTF);
    SetFontSize(18, 20);

    // list device space

    m = selected;

    if(Png_offset[m]) {

        i = -1;

        if(!mode_favourites || ((mode_favourites !=0) && favourites.list[m].index >= 0))
            for(i = 0; i < 11; i++)
                if((directories[(mode_favourites !=0) ? favourites.list[m].index : (currentdir + m)].flags & GAMELIST_FILTER)== (1<<i)) break;
        m = i;
    } else m = -1;

    x2 = 1200;
    for(n = 0; n < 2; n++) {
        for(i = 0; i < 11; i++) {

            if(((fdevices>>i) & 1)) {
                
                if(m == i) SetFontColor(0xafd836ff, 0x00000000); else SetFontColor(0xffffff44, 0x00000000);
                if(i==0)
                    x2= DrawFormatString(x2, 0, "hdd0: %.2fGB ", freeSpace[i]);
                else
                    x2= DrawFormatString(x2, 0, "usb00%c: %.2fGB ", 47 + i, freeSpace[i]);
            }

        }
    
        x2 = 848 -(x2 - 1200) - x; 
    }

    SetFontAutoCenter(0);
    SetFontSize(18, 20);

    SetFontColor(0xffffffff, 0x00000000);

    y += 24;
    
    i = 0;
    int flash2 = 0;
    #define MAX_FLASH 32
    if (frame_count & MAX_FLASH) flash2= (MAX_FLASH-1) - (frame_count & (MAX_FLASH-1)); else flash2= (frame_count & (MAX_FLASH-1));


    ///////////////////////

    int rel_posx[12];
    int rel_posy[12];
    int rel_widthx[12];
    int rel_widthy[12];

    int relx = 0;
    int centerx = 0;

    int icony = 128;

    int l;


    for(n = 0; n < 3; n++) 
        for(m = 0; m < 4; m++) {

        rel_widthy[i] = 142;
        rel_posy[i] = y + icony;
        if(Png_offset[i]) {
            if((directories[get_currentdir(i)].flags  & ((1<<11) | (1<<23)))== ((1<<11) | (1<<23))) 
                {Png_iscover[i] = -1;}
        else if(Png_iscover[i] < 1 && (directories[get_currentdir(i)].flags  & (1<<23))) 
            {Png_iscover[i] = -1;}
           
        }

        if((select_px == m && select_py == n)) {
            centerx = relx;
            if(Png_iscover[i] == -1) centerx+= (142 + 10)/2;
            else if(Png_iscover[i] == 1) centerx+= (124 + 10)/2;
            else centerx+= (162 + 10)/2;
        }

        if(Png_offset[i]) {

            if(Png_iscover[i] == -1)
                {rel_posx[i] = relx; rel_widthx[i] = 142; relx+= rel_widthx[i] + 10;}
            else if(Png_iscover[i] == 1)
                {rel_posx[i] = relx; rel_widthx[i] = 124; relx+= rel_widthx[i] + 10;}
            else
                {rel_posx[i] = relx; rel_posy[i]+= 16; rel_widthx[i] = 162; rel_widthy[i] = 142 - 16;relx+= rel_widthx[i] + 10;}
        } else 
            {rel_posx[i] = relx; rel_widthx[i] = 128; relx+= rel_widthx[i] + 10;}
        

        i++;
    }

    //////////////////////

    u32 color_line = 0x404040ff;

    // get_games_3

    if(centerx < 848/2) centerx = -(848/2 - centerx); else centerx -= 848/2;

    if((rel_posx[0] - centerx) > 30)  centerx = -30;

    float f3 = 0.0f;
    
    for(l = 0; l < 2; l++) {
        i = 0;

        for(n = 0; n < 3; n++) 
            for(m = 0; m < 4; m++) {
                int f = (select_px == m && select_py == n);
                float f2 = (int) f;

                if(l != 0 && f) {i++;continue;}
                if(l == 0 && !f) {i++;continue;}

                if(!mode_favourites && get_currentdir(i) >= ndirectories) break;
                
                f2 =  ((float) (flash2 *(select_px == m && select_py == n)))/((float) MAX_FLASH);


                if(f) {f2 = 3 + 2 *f2; f3 = f2;}
                
                
                if(Png_offset[i]) {
                   
                    if((directories[get_currentdir(i)].flags  & ((1<<11) | (1<<23)))== ((1<<11) | (1<<23))) {
                        //Png_iscover[i] = -1;
                        tiny3d_SetTextureWrap(0, Png_res_offset[5], Png_res[5].width, 
                                                 Png_res[5].height, Png_res[5].wpitch, 
                                                 TINY3D_TEX_FORMAT_A8R8G8B8,  TEXTWRAP_CLAMP, TEXTWRAP_CLAMP,1);
                    } else if(Png_iscover[i] < 1 && (directories[get_currentdir(i)].flags  & (1<<23))) { // add PSX iso
                        //Png_iscover[i] = -1;
                        tiny3d_SetTextureWrap(0, Png_res_offset[6], Png_res[6].width, 
                                                 Png_res[6].height, Png_res[6].wpitch, 
                                                 TINY3D_TEX_FORMAT_A8R8G8B8,  TEXTWRAP_CLAMP, TEXTWRAP_CLAMP,1);
                    } else  
                        tiny3d_SetTextureWrap(0, Png_offset[i], Png_datas[i].width, 
                                                 Png_datas[i].height, Png_datas[i].wpitch, 
                                                 TINY3D_TEX_FORMAT_A8R8G8B8,  TEXTWRAP_CLAMP, TEXTWRAP_CLAMP,1);


                    
                    DrawBox(rel_posx[i] - centerx - 4 * f2, rel_posy[i] - 4 * f2, !f ? 100 : 0, rel_widthx[i] + 8 * f2, rel_widthy[i] + 8 * f2, 0x00000028);
                    
                    DrawTextBoxLine(rel_posx[i] - centerx - 4 * f2, rel_posy[i] - 4 * f2, !f ? 100 : 0, rel_widthx[i] + 8 * f2, rel_widthy[i] + 8 * f2, 0xffffffff, color_line);

                    
                    DrawBoxShadow(rel_posx[i] - centerx - 4 * f2, y + icony - 4 * f2 + 142 + 8 * f2 + 8, !f ? 100 : 0, rel_widthx[i] + 8 * f2, (rel_widthy[i]  * 5 / 8 + 8 * f2), 0x00000028);
                    DrawTextBoxShadow(rel_posx[i] - centerx - 4 * f2, y + icony - 4 * f2 + 142 + 8 * f2 + 8, !f ? 100 : 0, rel_widthx[i] + 8 * f2, (rel_widthy[i]  * 5 / 8 + 8 * f2), 0x60606090);
                 
                    
                   // if((mode_favourites !=0) && favourites.list[i].index < 0) exit(0);
                    if((mode_favourites !=0) && favourites.list[i].index < 0) exit(0);
 
                    if(!mode_favourites || ((mode_favourites !=0) && favourites.list[i].index >= 0)) {
                        // ignore bluray icon
                        if((directories[get_currentdir(i)].flags  & ((1<<11) | (1<<23)))== ((1<<11) | (1<<23))) ;
                        // draw Bluray icon
                        else if((directories[get_currentdir(i)].flags  & GAMELIST_FILTER)== (1<<11)) {
                            tiny3d_SetTextureWrap(0, Png_res_offset[0], Png_res[0].width, 
                                                     Png_res[0].height, Png_res[0].wpitch, 
                                                     TINY3D_TEX_FORMAT_A8R8G8B8,  TEXTWRAP_CLAMP, TEXTWRAP_CLAMP,1);
                            DrawTextBox(rel_posx[i] - centerx - 4 * f2 + (rel_widthx[i] - 124)/2, rel_posy[i] + 4 + (rel_widthy[i] - 124)/2 - 4 * f2, !f ? 100 : 0,  (124) + 8 * f2, (124) + 8 * f2, 0xffffffcf);

                            DrawTextBoxShadow(rel_posx[i] - centerx - 4 * f2 + (rel_widthx[i] - 124)/2, y + icony - 4 * f2 + 142 + 8 * f2 + 8, !f ? 100 : 0,  (124) + 8 * f2, (124 * 5 / 8) + 8 * f2, 0x60606090);
                        } else 
                        // draw Usb icon    
                        if((directories[get_currentdir(i)].flags  & GAMELIST_FILTER)> 1) {
                            tiny3d_SetTextureWrap(0, Png_res_offset[1], Png_res[1].width, 
                            Png_res[1].height, Png_res[1].wpitch, 
                                TINY3D_TEX_FORMAT_A8R8G8B8,  TEXTWRAP_CLAMP, TEXTWRAP_CLAMP,1);

                            if(directories[get_currentdir(i)].splitted)
                                DrawTextBoxLine(rel_posx[i] - centerx - 4 * f2 + 4, rel_posy[i] + 4 - 4 * f2, !f ? 100 : 0, 32, 24, 0xff9999aa, color_line);
                            else
                                DrawTextBoxLine(rel_posx[i] - centerx - 4 * f2 + 4, rel_posy[i] + 4 - 4 * f2, !f ? 100 : 0, 32, 24, 0xffffffcf, color_line);
                            
                        }
                    }


                } else if(mode_favourites && favourites.list[i].title_id[0] != 0) {
                    DrawBoxLine(rel_posx[i] - centerx - 4 * f2, rel_posy[i] - 4 * f2, !f ? 100 : 0, 128 + 8 * f2, rel_widthy[i] + 8 * f2, 0xa0a0a028, color_line);
                    tiny3d_SetTextureWrap(0, Png_res_offset[2], Png_res[2].width, 
                        Png_res[2].height, Png_res[2].wpitch, 
                            TINY3D_TEX_FORMAT_A8R8G8B8,  TEXTWRAP_CLAMP, TEXTWRAP_CLAMP,1);
                        DrawTextBox(rel_posx[i] - centerx - 4 * f2, y  + 7 - 4 * f2 + icony, !f ? 100 : 0, 128 + 8 * f2, 128 + 8 * f2, 0xffffff3f);
                        DrawBoxShadow(rel_posx[i] - centerx - 4 * f2, y + icony - 4 * f2 + 142 + 8 * f2 + 8, !f ? 100 : 0, 128 + 8 * f2, (rel_widthy[i] * 5 / 8 + 8 * f2), 0x8f8f8f28);
                        DrawTextBoxShadow(rel_posx[i] - centerx - 4 * f2, y + icony - 4 * f2 + 142 + 8 * f2 + 8, !f ? 100 : 0, 128 + 8 * f2, (rel_widthy[i] * 5 / 8 + 8 * f2), 0x40404090);
                } else {
                    // draw Bluray icon with empty icon
                    if((directories[get_currentdir(i)].flags  & GAMELIST_FILTER)== (1<<11) &&
                        ((directories[get_currentdir(i)].flags  & ((1<<11) | (1<<23)))!= ((1<<11) | (1<<23)))) {

                        DrawBoxLine(rel_posx[i] - centerx - 4 * f2, rel_posy[i] - 4 * f2, !f ? 100 : 0, 128 + 8 * f2, rel_widthy[i] + 8 * f2, 0xa0a0a040, color_line);
                        DrawBoxShadow(rel_posx[i] - centerx - 4 * f2, y + icony - 4 * f2 + 142 + 8 * f2 + 8, !f ? 100 : 0, 128 + 8 * f2, (rel_widthy[i] * 5 / 8 + 8 * f2), 0x80808040);

                        tiny3d_SetTextureWrap(0, Png_res_offset[0], Png_res[0].width, 
                                                     Png_res[0].height, Png_res[0].wpitch, 
                                                     TINY3D_TEX_FORMAT_A8R8G8B8,  TEXTWRAP_CLAMP, TEXTWRAP_CLAMP,1);
                        DrawTextBox(rel_posx[i] - centerx - 4 * f2 + (rel_widthx[i] - 124)/2, rel_posy[i] + 4 + (rel_widthy[i] - 124)/2 - 4 * f2, !f ? 100 : 0,  (124) + 8 * f2, (124) + 8 * f2, 0xffffffcf);

                        DrawTextBoxShadow(rel_posx[i] - centerx - 4 * f2 + (rel_widthx[i] - 124)/2, y + icony - 4 * f2 + 142 + 8 * f2 + 8, !f ? 100 : 0,  (124) + 8 * f2, (124 * 5 / 8) + 8 * f2, 0x60606090);

                    } else {
                        DrawBoxLine(rel_posx[i] - centerx - 4 * f2, rel_posy[i] - 4 * f2, !f ? 100 : 0, 128 + 8 * f2, rel_widthy[i] + 8 * f2, 0xa0a0a040, color_line);
                        DrawBoxShadow(rel_posx[i] - centerx - 4 * f2, y + icony - 4 * f2 + 142 + 8 * f2 + 8, !f ? 100 : 0, 128 + 8 * f2, (rel_widthy[i] * 5 / 8 + 8 * f2), 0x80808040);
                    }
                }
                         
                
            i++;   
            }
    }

    i = selected;

    if(flash) {

        int png_on = 0;

        //DrawBox(x + 200 * select_px - 4, y + select_py * 150 - 4 , 0, 200, 150, 0xa0a06080);
            

        if(mode_favourites >= 65536) {

            if(mode_favourites < 131072) {

                if(Png_offset[12]) {
                    tiny3d_SetTextureWrap(0, Png_offset[12], Png_datas[12].width, 
                        Png_datas[12].height, Png_datas[12].wpitch, 
                            TINY3D_TEX_FORMAT_A8R8G8B8,  TEXTWRAP_CLAMP, TEXTWRAP_CLAMP,1);
                    png_on = 1;
                }
            }
            else {
                i = mode_favourites - 131072;

                if(i>= 0 && i < 12) {
                    if(!Png_offset[i] && favourites.list[i].title_id[0] != 0) {
                        tiny3d_SetTextureWrap(0, Png_res_offset[2], Png_res[2].width, 
                            Png_res[2].height, Png_res[2].wpitch, 
                                TINY3D_TEX_FORMAT_A8R8G8B8,  TEXTWRAP_CLAMP, TEXTWRAP_CLAMP,1);
                        png_on = 1;
                            
                    } else if(Png_offset[i]){
                        png_on = 1;
                        tiny3d_SetTextureWrap(0, Png_offset[i], Png_datas[i].width, 
                        Png_datas[i].height, Png_datas[i].wpitch, 
                            TINY3D_TEX_FORMAT_A8R8G8B8,  TEXTWRAP_CLAMP, TEXTWRAP_CLAMP,1);
                    }
                }
            }


            int ii = select_px + 4 * select_py;

            if(png_on)
                DrawTextBox(rel_posx[ii] - centerx - 4 * f3, rel_posy[ii] - 4 * f3 , 0 , rel_widthx[ii] + 8 * f3, rel_widthy[ii] + 8 * f3, 0x8fff8fcf);
            else
                DrawBox(rel_posx[ii] - centerx - 4 * f3, rel_posy[ii] - 4 * f3 , 0 , rel_widthx[ii] + 8 * f3, rel_widthy[ii] + 8 * f3, 0x8fff8fcf);
        }
    
    }

    SetFontColor(0xffffffff, 0x00000000);

    // display temp
    if((frame_count & 0x100)) {
        static u32 temp = 0;
        static u32 temp2 = 0;
        int y2;

        if(temp==0 || (frame_count & 0x1f) == 0x0 ) {
            sys_game_get_temperature(0, &temp);
            sys_game_get_temperature(1, &temp2);
        }

        SetCurrentFont(FONT_TTF);
        SetFontSize(20, 20);
        
        x2= DrawFormatString(1024, 0, " Temp CPU: 99ºC RSX: 99ºC ");

        y2= y + 3 * 150 - 4 + 12;
        SetFontColor(0xffffffff, 0x00000000);
        x2= DrawFormatString(x + 4 * 200 - (x2 - 1024) - 12 , y2, " Temp CPU: ");
        if(temp < 80) SetFontColor(0xfff000ff, 0x00000000); else SetFontColor(0xff0000ff, 0x00000000);
        x2= DrawFormatString(x2, y2, "%uºC",  temp);
        SetFontColor(0xffffffff, 0x00000000);
        x2= DrawFormatString(x2, y2, " RSX: ");
        if(temp2 < 75) SetFontColor(0xfff000ff, 0x00000000); else SetFontColor(0xff0000ff, 0x00000000);
        x2= DrawFormatString(x2, y2, "%uºC ", temp2);

        SetFontColor(0xffffffff, 0x00000000);
    }
    else if(Png_offset[i])
    {
        SetCurrentFont(FONT_TTF);
        SetFontSize(20, 20);
        x2= DrawFormatString(1024, 0, " %s ", language[DRAWSCREEN_SOPTIONS]);
        DrawFormatString(x + 4 * 200 - (x2 - 1024) - 12 , y + 3 * 150 - 2, " %s ", language[DRAWSCREEN_SOPTIONS]);
   
    }
    else if(mode_favourites && mode_favourites < 65536 && favourites.list[i].title_id[0] != 0) 
    {
        SetCurrentFont(FONT_TTF);
        SetFontSize(20, 20);
        x2= DrawFormatString(1024, 0, " %s ", language[DRAWSCREEN_SDELETE]);
        DrawFormatString(x + 4 * 200 - (x2 - 1024) - 12 , y + 3 * 150 - 2, " %s ", language[DRAWSCREEN_SDELETE]);
    }
    else
    {
        //DrawBox(x + 200 * select_px , y + select_py * 150 , 0, 192, 142, 0x404040a0);
        SetCurrentFont(FONT_TTF); // get default
        SetFontSize(20, 20);
    }
    
    if(!(frame_count & 0x100)) {
        x2= DrawFormatString(1024, 0, " %s ", language[DRAWSCREEN_STGLOPT]);
        DrawFormatString(x + 4 * 200 - (x2 - 1024) - 12 , y + 3 * 150 + 18, " %s ", language[DRAWSCREEN_STGLOPT]);
    }
    

    // draw game name
    i = selected;

    DrawBox(x, y + 3 * 150, 0, 200 * 4 - 8, 40, 0x00000028);

    SetFontColor(0xffffffee, 0x00000000);

    if((Png_offset[i] && !mode_favourites) || (mode_favourites && favourites.list[i].title_id[0] != 0)) {

        
        u32 str_color = 0xffffffee;

        if(mode_favourites) {

            if(strncmp((char *) string_title_utf8, favourites.list[i].title, 64)) {
                strncpy((char *) string_title_utf8, favourites.list[i].title, 128);
                update_title_utf8 = 1;
            }

        } else if((directories[(currentdir + i)].flags  & GAMELIST_FILTER)== (1<<11)) {
         
            str_color = 0xafd836ee;
            if(strncmp((char *) string_title_utf8, bluray_game, 64)) {
                strncpy((char *) string_title_utf8, bluray_game, 128);
                update_title_utf8 = 1;
            }

        } else {

            if(strncmp((char *) string_title_utf8, directories[(currentdir + i)].title, 64)) {
                strncpy((char *) string_title_utf8, directories[(currentdir + i)].title, 128);
                update_title_utf8 = 1;
            }
        }

        n = 0; 
        while(string_title_utf8[n]) {
            if(string_title_utf8[n] == 13 || string_title_utf8[n] == 10) string_title_utf8[n] = '/';
            n++;
        }

        SetCurrentFont(FONT_TTF);
        SetFontSize(16, 32);
        SetFontAutoCenter(1);
        SetFontColor(str_color, 0x00000020);
        DrawFormatString(0 , y + 80 - 32, " %s ", string_title_utf8);
        SetFontColor(0xffffffff, 0x00000000);
/*
        if(update_title_utf8) {
            width_title_utf8 = Render_String_UTF8(ttf_texture, 768, 32, string_title_utf8, 16, 24);
            update_title_utf8 = 0;
        }

        tiny3d_SetTextureWrap(0, tiny3d_TextureOffset(ttf_texture), 768, 
                32, 768 * 2, 
                TINY3D_TEX_FORMAT_A4R4G4B4,  TEXTWRAP_CLAMP, TEXTWRAP_CLAMP,1);
        
        DrawTextBox(x + 3, y + 3 * 150 , 0, 768, 32, str_color);
        */
       

        SetFontAutoCenter(0);

        load_gamecfg (get_currentdir(i)); // refresh game info

        
        x2 = 0;

        if((game_cfg.useBDVD) || (game_cfg.direct_boot == 2))
        {
            tiny3d_SetTextureWrap(0, Png_res_offset[0], Png_res[0].width, 
                Png_res[0].height, Png_res[0].wpitch, 
                TINY3D_TEX_FORMAT_A8R8G8B8,  TEXTWRAP_CLAMP, TEXTWRAP_CLAMP,1);

            DrawTextBox(x + x2, y + 3 * 150 - 48, 0, 32, 32, 0xffffff99);

            x2+= 40;
        }
        
        if(game_cfg.direct_boot)
        {
            tiny3d_SetTextureWrap(0, Png_res_offset[3], Png_res[3].width, 
                Png_res[3].height, Png_res[3].wpitch, 
                TINY3D_TEX_FORMAT_A8R8G8B8,  TEXTWRAP_CLAMP, TEXTWRAP_CLAMP,1);

            DrawTextBox(x + x2,  y + 3 * 150 - 48, 0, 32, 32, 0xffffffff); 
            x2+= 40;
        }

        n = (directories[get_currentdir(i)].flags & 1) ?  game_cfg.bdemu : game_cfg.bdemu_ext;

        SetFontColor(0xffffffee, 0x00000000);
        SetCurrentFont(FONT_TTF);
        SetFontSize(12, 24);

        if(n == 1) x2 = DrawString(x + x2,  y + 3 * 150 - 48 + 4, "BD Emu");

        if(n == 2) x2 = DrawString(x + x2,  y + 3 * 150 - 48 + 4, "LIBFS");


    }

    SetFontColor(0xffffffee, 0x00000000);
    SetCurrentFont(FONT_TTF);
    SetFontSize(12, 24);

    SetFontAutoCenter(1);

    if(sort_mode == 1)
        DrawString(0,  y + 3 * 150 - 48 + 4, "PS3 > PSX");
    else if(sort_mode == 2)
        DrawString(0,  y + 3 * 150 - 48 + 4, "PSX > PS3");
    else
        DrawString(0,  y + 3 * 150 - 48 + 4, "PS3 - PSX");

    SetFontAutoCenter(0);

// draw box config
    tiny3d_SetPolygon(TINY3D_LINE_STRIP);
       
    tiny3d_VertexPos(x - 4    , y + 3 * 150 - 52    , 0);
    tiny3d_VertexColor(0x808080ff);

    tiny3d_VertexPos(x + 124 + 8, y + 3 * 150 - 52    , 0);

    tiny3d_VertexPos(x + 124 + 8, y + 3 * 150 - 52 + 40, 0);

    tiny3d_VertexPos(x - 4   , y + 3 * 150 - 52 + 40, 0);

    tiny3d_VertexPos(x - 4   , y + 3 * 150 - 52    , 0);

    tiny3d_End();

    if(flash && ftp_inited) {
        tiny3d_SetTextureWrap(0, Png_res_offset[4], Png_res[4].width, 
            Png_res[4].height, Png_res[4].wpitch, 
            TINY3D_TEX_FORMAT_A8R8G8B8,  TEXTWRAP_CLAMP, TEXTWRAP_CLAMP,1);

        DrawTextBox(200 * 4 -32, y + 3 * 150 - 48, 0, 64, 32, 0xffffffff); 
    }

    //SetCurrentFont(FONT_DEFAULT);

    tiny3d_Flip();

}

void gui_control()
{

    int i, n;

    int selected = select_px + select_py * 4;


    ps3pad_read();

    if(autolaunch == LAUCHMODE_TOCHECK) {

        int n;

        for(n = 0; n < 5; n++) {
            
            if((new_pad | old_pad) & BUTTON_L1) {
                sprintf(temp_buffer, "%s/lastgame", self_path);
                int size;
                char * mem = LoadFile(temp_buffer, &size);
                if(mem && size == 72) {
                    

                    for(n = 0; n < ndirectories; n++) {
                        if(!strcmp(directories[n].title_id, mem) && !memcmp(&directories[n].flags, mem + 68, 4)) {
                            autolaunch = n; n = 100; break;
                        }
                    }
                }
                if(mem) free(mem);
                if(autolaunch >= LAUCHMODE_STARTED) {
                    load_gamecfg(autolaunch);
                    mode_favourites = 0;
                    goto autolaunch_proc;
                }
            }

            ps3pad_read();
            usleep(20000);
        }
    }

    autolaunch = LAUCHMODE_CHECKED;

    if(mode_favourites && (old_pad & BUTTON_L2) && (new_pad & BUTTON_TRIANGLE)) {
        mode_favourites = 131072 | (selected);
        return;
    }

    if(new_pad & BUTTON_TRIANGLE) {
        if(mode_favourites >= 65536) {
            mode_favourites = 1;

        } else {
            if(!test_ftp_working()) {
                if(DrawDialogYesNo(language[DRAWSCREEN_EXITXMB])==1) {exit_program = 1; return;}
                if(DrawDialogYesNo("Restart the PS3 System?\n\nReiniciar la PS3?")==1) {set_install_pkg = 1; game_cfg.direct_boot=0; exit(0);}
            }
            
        }
    }

    if(new_pad & BUTTON_SQUARE) {
       
        if(!mode_homebrew) {
            if(!bdvd_ejected)
                Eject_BDVD(NOWAIT_BDVD | EJECT_BDVD);
            else
                Eject_BDVD(NOWAIT_BDVD | LOAD_BDVD);
        }
            
    }

    if(new_pad & (BUTTON_CROSS | BUTTON_CIRCLE)) {
        i = selected;

        if(mode_favourites >= 131072) { // swap favourites
            
            entry_favourites swap = favourites.list[i];

            favourites.list[i] = favourites.list[mode_favourites - 131072]; favourites.list[mode_favourites - 131072] = swap;

            sprintf(temp_buffer, "%s/config/", self_path);
            SaveFavourites(temp_buffer, mode_homebrew);

            mode_favourites = 1;
            get_games();

            return;     
        }

        if(mode_favourites >= 65536) { // insert favourites
            
            DeleteFavouritesIfExits(directories[mode_favourites - 65536].title_id);
            AddFavourites(i, directories, mode_favourites - 65536);

            sprintf(temp_buffer, "%s/config/", self_path);
            SaveFavourites(temp_buffer, mode_homebrew);

            mode_favourites = 1;
            get_games();

            return;     
        }

        if(test_ftp_working()) return;

        if(Png_offset[i]) {
            if(mode_favourites != 0 && favourites.list[i].index < 0) {
                DrawDialogOK(language[DRAWSCREEN_CANRUNFAV]);return;
            } else {

                autolaunch_proc:
                if(autolaunch >= LAUCHMODE_STARTED)
                    currentgamedir = autolaunch;
                else
                    currentgamedir = (mode_favourites !=0) ? favourites.list[i].index : (currentdir + i);

                if(currentgamedir < 0 || currentgamedir >= ndirectories) return;

                reset_sys8_path_table();

                int use_cache = 0;

                if(!(directories[currentgamedir].flags & (1<<11)) && !((directories[currentgamedir].flags>>31) & 1)) {
               
                    struct stat s;
                 
                    sprintf(temp_buffer, "%s/PS3_GAME/USRDIR/EBOOT.BIN", directories[currentgamedir].path_name);

                    if(!stat(temp_buffer, &s)) {
                        int r =  patch_exe_error_09(temp_buffer);
                        if(r == 1) {
                            pause_music(1);

                            test_game(currentgamedir);
                        } else if(r == -1) {
                            DrawDialogOKTimer("This game requires a higher CFW or rebuild the SELFs/SPRX\n\nEste juego requieren un CFW superior or reconstruir los SELFs/SPRX", 2000.0f);
                            return;
                        }
                    }


                }

                if(!((directories[currentgamedir].flags & ((1<<11) | (1<<23))) == ((1<<11) | (1<<23))) || // !psx CD
                   ((directories[currentgamedir].flags>>31) & 1)) { // !homebrew

                    sprintf(temp_buffer, "%s/lastgame", self_path);
                    memset(temp_buffer + 1024, 0, 72);
                    memcpy(temp_buffer + 1024, directories[currentgamedir].title_id, 65);
                    memcpy(temp_buffer + 1092, &directories[currentgamedir].flags, 4);

                    SaveFile(temp_buffer, temp_buffer + 1024, 72);

                    if(!(directories[currentgamedir].flags & (1<<11)) && lv2peek(0x80000000000004E8ULL))
                        set_BdId(currentgamedir);
                }

                if(directories[currentgamedir].splitted == 1) {
                    if( payload_mode >= ZERO_PAYLOAD )
                    {
                        sprintf(temp_buffer, "%s/cache/%s/%s", self_path, 
                        directories[currentgamedir].title_id, "/paths.dir");

                        struct stat s;
                    
                        if(stat(temp_buffer, &s)<0) {
                            sprintf(temp_buffer + 1024, "%s\n\n%s", 
                            directories[currentgamedir].title, language[DRAWSCREEN_MARKNOTEXEC]);
                            DrawDialogOK(temp_buffer + 1024);
                            
                            copy_to_cache(currentgamedir, self_path);
    
                            sprintf(temp_buffer, "%s/cache/%s/%s", self_path, 
                            directories[currentgamedir].title_id, "/paths.dir");
                            if(stat(temp_buffer, &s)<0) return; // cannot launch without cache files
                        }
                     

                        use_cache = 1;
                    }
                    else
                    {
                        sprintf(temp_buffer, 
                            "%s\n\n%s", 
                            directories[get_currentdir(i)].title, language[DRAWSCREEN_MARKNOTEX4G]);
                        DrawDialogOK(temp_buffer);return;
                    }
                }

                /// cache
                if(use_cache && ((game_cfg.bdemu==1 && (directories[currentgamedir].flags & 1)) || 
                     (!(directories[currentgamedir].flags & 1) && game_cfg.bdemu_ext==1))) {
         
                    DrawDialogOKTimer("BD Emu cannot work with big file cache datas\n\nBD Emu no puede trabajar con big file en cache de datos", 2000.0f);
                    return;     
                }
                   
                
                if(game_cfg.exthdd0emu) {

                    if((directories[currentgamedir].flags & 2046) != 0) {
                        
                        for(n = 1; n < 11 ; n++) if((directories[currentgamedir].flags  & GAMELIST_FILTER) == (1 << n)) break;
                        sprintf(temp_buffer, "/dev_usb00%c/GAMEI", 47 + n);
                        mkdir_secure(temp_buffer);
                        add_sys8_path_table(self_path, self_path);
                        
                        if(((directories[currentgamedir].flags & 1) && game_cfg.bdemu) || (!(directories[currentgamedir].flags & 1) && game_cfg.bdemu_ext)) 
                            add_sys8_path_table("/dev_hdd0/game", "/dev_bdvd/GAMEI");
                        else
                            add_sys8_path_table("/dev_hdd0/game", temp_buffer);

                    } else if((fdevices & 2046) != 0){

                        for(n = 1; n < 11; n++) { // searching directory
                            if(fdevices & (1 << n)) {
                                DIR  *dir;
                                
                                sprintf(temp_buffer, "/dev_usb00%c/GAMEI", 47 + n);
                                dir = opendir (temp_buffer);
                                if(dir) {
                                    closedir (dir);
                                    
                                    add_sys8_path_table(self_path, self_path);
                                    add_sys8_path_table("/dev_hdd0/game", temp_buffer);
                                    break;
                                }
                            }
                        }
                        
                        if(n == 11) { // directory not found, Asking to create one
                        
                             for(n = 1; n < 11 ; n++) {
                                if(fdevices & (1 << n)) {
                                    sprintf(temp_buffer, "%s\n\n%s%c?"
                                            , language[DRAWSCREEN_GAMEINOFMNT], language[DRAWSCREEN_GAMEIASKDIR], 47 + n);
                                    if(DrawDialogYesNo(temp_buffer) == 1) {
                                        sprintf(temp_buffer, "/dev_usb00%c/GAMEI", 47 + n);
                                        mkdir_secure(temp_buffer);
                                        add_sys8_path_table(self_path, self_path);
                                        add_sys8_path_table("/dev_hdd0/game", temp_buffer);
                                        break;
                                    }
                                }
                             }
                             
                             if(n == 11) {
                                 sprintf(temp_buffer, "%s\n\n%s", language[DRAWSCREEN_GAMEICANTFD], language[DRAWSCREEN_GAMEIWLAUNCH]);
                                 if(DrawDialogYesNo(temp_buffer) != 1) return;
                             }
                        }
                    }

                    if((fdevices & 2046) == 0) {
                        sprintf(temp_buffer, "%s\n\n%s", language[DRAWSCREEN_GAMEICANTFD], language[DRAWSCREEN_GAMEIWLAUNCH]);
                        if(DrawDialogYesNo(temp_buffer) != 1) return;
                    }

                }

                if((game_cfg.useBDVD && (fdevices & 2048) == 0) || (game_cfg.direct_boot == 2))
                {
                    load_from_bluray |= 1;
                    if(!noBDVD && check_disc() == -1)
                        return;
                }
                
                if(!(directories[currentgamedir].flags & 2048))
                    param_sfo_util(directories[currentgamedir].path_name, (game_cfg.updates != 0));

                if(!game_cfg.ext_ebootbin) sys8_path_table(0LL);
                else {
                    
                   
                    set_device_wakeup_mode(bdvd_is_usb ? 0xFFFFFFFF : directories[currentgamedir].flags);

                    sprintf(temp_buffer, "%s/self/%s.BIN", self_path, 
                        directories[get_currentdir(i)].title_id);

                    FILE *fp = fopen(temp_buffer, "rb");

                    if(!fp) {
                        sprintf(temp_buffer, " %s.BIN\n %s\n\n%s", 
                            directories[currentgamedir].title_id, language[DRAWSCREEN_EXTEXENOTFND], language[DRAWSCREEN_EXTEXENOTCPY]);
                        DrawDialogOK(temp_buffer);
                        goto skip_sys8;
                    } else {

                        fclose(fp);
                        add_sys8_path_table("/dev_bdvd/PS3_GAME/USRDIR/EBOOT.BIN", temp_buffer);
                    }
                
                }
                
                load_from_bluray = game_cfg.useBDVD;

                if(!game_cfg.useBDVD || noBDVD) sys8_sys_configure(CFG_XMB_DEBUG); else sys8_sys_configure(CFG_XMB_RETAIL);
                
              
                sys8_sys_configure(CFG_UNPATCH_APPVER + (game_cfg.updates != 0));
                
                if((game_cfg.bdemu && (directories[currentgamedir].flags & 1)) || 
                     (!(directories[currentgamedir].flags & 1) && game_cfg.bdemu_ext==1)) {
                    
                    load_from_bluray |= 1;

                    if(!noBDVD && check_disc() == -1)
                        return;

                    if(!noBDVD)
                        sys_fs_mount("CELL_FS_IOS:BDVD_DRIVE", "CELL_FS_ISO9660", "/dev_ps2disc", 1);

                    if((game_cfg.bdemu==2 && (directories[currentgamedir].flags & 1)) || 
                     (!(directories[currentgamedir].flags & 1) && game_cfg.bdemu_ext==2)) { // new to add BD-Emu 2

                         n = 0;
                       
                    }
                    else {
                        n = move_origin_to_bdemubackup(directories[currentgamedir].path_name);
                        sprintf(temp_buffer, "%s/PS3_DISC.SFB", directories[currentgamedir].path_name);
                        add_sys8_path_table("/dev_bdvd/PS3_DISC.SFB", temp_buffer);
                       
                    }
                    if(n < 0)
                    {
                        //syscall36("/dev_bdvd"); // in error exits
                        //sys8_perm_mode((u64) 0);

                        add_sys8_bdvd(NULL, NULL);

                        //if(game_cfg.ext_ebootbin)
                        build_sys8_path_table(); //prepare extern eboot

                        exit_program = 1; 
                        return;
                    }

                    is_ps3game_running = 1;

                   // if(n == 1) game_cfg.bdemu = 0; // if !dev_usb... bdemu is not usable
                }

               
                // HDD BDEMU-LIBFS / USB BDEMU
                if(((game_cfg.bdemu && (directories[currentgamedir].flags & 1)) || 
                     (!(directories[currentgamedir].flags & 1) && game_cfg.bdemu_ext==1)) && 
                    patch_bdvdemu(directories[currentgamedir].flags & GAMELIST_FILTER) == 0) {
                    
                    
                    // syscall36("//dev_bdvd"); // for hermes special flag see syscall36-3.41-hermes "//"
                    add_sys8_bdvd(NULL, NULL);
                    
                    //we dont want libfs on USB devices
                    if(is_libfs_patched() && (game_cfg.bdemu && (directories[currentgamedir].flags & 1))) {
                        sprintf(temp_buffer + 1024, "%s/libfs_patched.sprx", self_path);
                        add_sys8_path_table("/dev_flash/sys/external/libfs.sprx", temp_buffer + 1024);
                        sys8_pokeinstr(0x80000000007EF220ULL, 0x45737477616C6420ULL);

                    }

                
                    // HDD LIBFS
                   if((game_cfg.bdemu == 2 && (directories[currentgamedir].flags & 1))) { // new to add BD-Emu 2
                        mount_custom(directories[currentgamedir].path_name);
                       
                        if(!game_cfg.useBDVD) {

                            sprintf(temp_buffer, "%s/PS3_DISC.SFB", directories[currentgamedir].path_name);
                            add_sys8_path_table("/app_home/PS3_DISC.SFB", temp_buffer);

                            sprintf(temp_buffer, "%s/PS3_GAME", directories[currentgamedir].path_name);
                            add_sys8_path_table("/app_home/PS3_GAME", temp_buffer);

                            sprintf(temp_buffer, "%s/PS3_GAME/USRDIR", directories[currentgamedir].path_name);
                            add_sys8_path_table("/app_home/USRDIR", temp_buffer);

                            sprintf(temp_buffer, "%s/PS3_GAME/USRDIR", directories[currentgamedir].path_name);
                            add_sys8_path_table("/app_home", temp_buffer);

                        }
                        add_sys8_bdvd(directories[currentgamedir].path_name, NULL);

                        //syscall36(directories[currentgamedir].path_name);
                   } else {
                        mount_custom(directories[currentgamedir].path_name);
                        if(!game_cfg.useBDVD) {
                            sprintf(temp_buffer, "%s/PS3_DISC.SFB", "/dev_bdvd");
                            add_sys8_path_table("/app_home/PS3_DISC.SFB", temp_buffer);

                            sprintf(temp_buffer, "%s/PS3_GAME", "/dev_bdvd");
                            add_sys8_path_table("/app_home/PS3_GAME", temp_buffer);

                            sprintf(temp_buffer, "%s/PS3_GAME/USRDIR", "/dev_bdvd");
                            add_sys8_path_table("/app_home/USRDIR", temp_buffer);

                            sprintf(temp_buffer, "%s/PS3_GAME/USRDIR", "/dev_bdvd");
                            add_sys8_path_table("/app_home", temp_buffer);

                        }

                   }

                   
                }
                else {

                // USB LIBFS (for game cacheds)
                if(is_libfs_patched() && (!(directories[currentgamedir].flags & 1) && game_cfg.bdemu_ext==2) &&
                    !(directories[currentgamedir].flags & 2048)) { // only with external bdemu LIBFS for cached files
                            sprintf(temp_buffer + 1024, "%s/libfs_patched.sprx", self_path);
                            add_sys8_path_table("/dev_flash/sys/external/libfs.sprx", temp_buffer + 1024);
                            sys8_pokeinstr(0x80000000007EF220ULL, 0x45737477616C6420ULL);

                }
                
                if(use_cache) {
                       
                    sprintf(temp_buffer + 1024, "%s/cache/%s/%s", self_path,  //check replace (1024)
                        directories[currentgamedir].title_id, "/paths.dir");
                        
                        char *path = LoadFile(temp_buffer + 1024, &n);
                        char *mem = path;

                        if(path && path[0x400]==0 && path[0x420]!=0) { // repair bug in some Iris Manager versions

                            memcpy(&path[0x400], &path[0x420], 0x380);
                            SaveFile(temp_buffer + 1024, path, n);
                            
                        }

                        n = n & ~2047; // if file truncated break bad datas...

                        if(path)
                        {
                            while(n > 0)
                            {
                                char *t = path;
                                
                                sprintf(temp_buffer + 1024, "%s/cache/%s/%s", self_path, 
                                directories[currentgamedir].title_id, path + 0x400);
                                
                                 
                                path = strstr(path, "PS3_GAME/");
                               
                                sprintf(temp_buffer, "/dev_bdvd/%s", path);

                                add_sys8_path_table(temp_buffer, temp_buffer + 1024);

                                //DrawDialogOK(temp_buffer);
                                //DrawDialogOK(temp_buffer + 1024);

                                path = t + 2048;
                                n   -= 2048;
                            }
                            free(mem); 

                        }
                    }
                    
                    if((directories[currentgamedir].flags & (1<<23)) == (1<<23)) { // add PSX iso

                        if(!lv2_patch_storage && (directories[currentgamedir].flags & (1<<11))) {
                            DrawDialogOKTimer("PSX Unsupported", 2000.0f);
                            return;
                        }

                        //syscall36("/dev_bdvd");
                        add_sys8_bdvd(NULL, NULL);

                        if(!(directories[currentgamedir].flags & (1<<11))) {
                            
                            if(lv2peek(0x80000000000004E8ULL)) syscall_40(1, 0); // disables PS3 Disc-less 
                            
                            // load PSX options
                            LoadPSXOptions(directories[currentgamedir].path_name);
                               
                            if(psx_iso_prepare(directories[currentgamedir].path_name, directories[currentgamedir].title)==0)  {
                                DrawDialogOKTimer("PSX Game not detected!", 2000.0f);
                                return;
                            }
                            
                        } else psx_cd_with_cheats();
                        
                        set_device_wakeup_mode(bdvd_is_usb ? 0xFFFFFFFF : directories[currentgamedir].flags);

                        psx_launch();
                    } else if((directories[currentgamedir].flags>>31) & 1) { // is homebrew
                    
                        reset_sys8_path_table();
                        add_sys8_bdvd(NULL, "/dev_kk");

                        sprintf(temp_buffer, "%s/PARAM.SFO", directories[currentgamedir].path_name);
                        
                        if(homelaun==2) {// test if same title id to skip homelaun pass

                            if(!parse_param_sfo_id("/dev_hdd0/game/HOMELAUN1/PARAMX.SFO", temp_buffer + 1024))
                                if(strcmp(directories[currentgamedir].title_id, temp_buffer + 1024)) {homelaun=1;}
                        }
                        
                        i=param_sfo_patch_category_to_cb(temp_buffer, "/dev_hdd0/game/HOMELAUN1/PARAMX.SFO");

                        if(i==-1) {
                            DrawDialogOK("Error: I cannot find PARAM.SFO for HOMELAUN1 (game broken)");
                            goto skip_homebrew;
                        }

                        if(i==-2) {
                            DrawDialogOK("Error: External USB Loader not found! (install HOMELAUN1)");
                            goto skip_homebrew;
                        }
                       
                        sprintf(temp_buffer, "%s", directories[currentgamedir].path_name);

                        sprintf(temp_buffer + 1024, "%s/homelaunc1.bin", self_path);
                        sprintf(temp_buffer + 2048, "%s/homelaunc1.bin", self_path);
                        SaveFile(temp_buffer + 2048, temp_buffer, 2048);

                        i=strlen(directories[currentgamedir].path_name); 
                        while(directories[currentgamedir].path_name[i]!='/') i--;

                        sprintf(temp_buffer + 1024, "/dev_hdd0/game%s", &directories[currentgamedir].path_name[i]);

                        if(homelaun==2) add_sys8_path_table("/dev_hdd0/game/HOMELAUN1/ICON0.PNG", "//dev_hdd0/game/HOMELAUN1/GICON0.PNG");
                        else add_sys8_path_table("/dev_hdd0/game/HOMELAUN1/ICON0.PNG", "//dev_hdd0/game/HOMELAUN1/BICON0.PNG");
                        
                        if(homelaun!=2)
                            add_sys8_path_table("/dev_hdd0/game/HOMELAUN1/USRDIR/EBOOT.BIN", "//dev_hdd0/game/HOMELAUN1/USRDIR/EBOOT.BIN");
                        
                        if(homelaun!=1) add_sys8_path_table("/dev_hdd0/game/HOMELAUN1/PARAM.SFO", "//dev_hdd0/game/HOMELAUN1/PARAMX.SFO");
                        else add_sys8_path_table("/dev_hdd0/game/HOMELAUN1/PARAM.SFO", "//dev_hdd0/game/HOMELAUN1/PARAM.SFO");
                        
                        add_sys8_path_table("/dev_hdd0/game/HOMELAUN1/path.bin", temp_buffer + 2048);
                        if(homelaun==2) add_sys8_path_table("/dev_hdd0/game/HOMELAUN1/path2.bin", temp_buffer + 2048);
                        
                        add_sys8_path_table("/dev_hdd0/game/HOMELAUN1", temp_buffer);

                        for(i = 0; i < 11; i++) if((directories[currentgamedir].flags>>i) & 1) break;
                        
                        if(i!=0) 
                            add_sys8_path_table(temp_buffer + 1024, temp_buffer);

                        build_sys8_path_table();
                        set_device_wakeup_mode(bdvd_is_usb ? 0xFFFFFFFF : directories[currentgamedir].flags);
                        game_cfg.direct_boot=0;
                        exit(0);
                        
                        skip_homebrew: 
                            ;
                   
                    } else {
                        mount_custom(directories[currentgamedir].path_name);
                        if(!game_cfg.useBDVD) {
                            sprintf(temp_buffer, "%s/PS3_DISC.SFB", directories[currentgamedir].path_name);
                            add_sys8_path_table("/app_home/PS3_DISC.SFB", temp_buffer);

                            sprintf(temp_buffer, "%s/PS3_GAME", directories[currentgamedir].path_name);
                            add_sys8_path_table("/app_home/PS3_GAME", temp_buffer);

                            sprintf(temp_buffer, "%s/PS3_GAME/USRDIR", directories[currentgamedir].path_name);
                            add_sys8_path_table("/app_home/USRDIR", temp_buffer);

                            sprintf(temp_buffer, "%s/PS3_GAME/USRDIR", directories[currentgamedir].path_name);
                            add_sys8_path_table("/app_home", temp_buffer);

                        }

                        is_ps3game_running = 1;

                        //syscall36(directories[currentgamedir].path_name); // is bdvd game

                        add_sys8_bdvd(directories[currentgamedir].path_name, NULL);

                    }
                }
                
                if(noBDVD) {
                    struct stat s;
                    patch_bdvdemu((1<<15)/*directories[currentgamedir].flags & GAMELIST_FILTER*/);

                    if(load_from_bluray) {
   
                        if((firmware & 0xF)==0xD) 
                            sprintf(temp_buffer, "%s/explore_plugin_%xdex.sprx", self_path, firmware>>4);
                        else
                            sprintf(temp_buffer, "%s/explore_plugin_%x.sprx", self_path, firmware>>4);

                        if(stat(temp_buffer, &s)<0) 
                            {
                            
                             if(noBDVD == 2) strcat(temp_buffer, " not found\n\nIt reduces the game compatibility");
                             else strcat(temp_buffer, " not found\n\npath from /app_home"); 
                             DrawDialogOKTimer(temp_buffer+strlen(self_path)+1, 2000.0f);
                            }
                        else
                            add_sys8_path_table("/dev_flash/vsh/module/explore_plugin.sprx", temp_buffer);
                    }

                }

               
                build_sys8_path_table();

                exit_program = 1; 

                if(game_cfg.direct_boot) game_cfg.direct_boot = 555;

                set_device_wakeup_mode(bdvd_is_usb ? 0xFFFFFFFF : directories[currentgamedir].flags);
                
                skip_sys8: 
                    return;
            }
        }
    }

    if(new_pad & BUTTON_START) {

        if(options_locked)
            DrawDialogOKTimer("Locked by Parental Control", 2000.0f);
        else {

            select_option = 0;
            menu_screen = 3; return;
        }
    }

    if(new_pad & BUTTON_SELECT) {
        if(options_locked)
            DrawDialogOKTimer("Locked by Parental Control", 2000.0f);
        else {
            i = selected;

            select_option = 0;
            
            if(!Png_offset[i] && mode_favourites && mode_favourites < 65536 && favourites.list[i].title_id[0] != 0) {
                DeleteFavouritesIfExits(favourites.list[i].title_id);

                sprintf(temp_buffer, "%s/config/", self_path);
                SaveFavourites(temp_buffer, mode_homebrew);

                if(mode_favourites && !havefavourites) mode_favourites = 0;
                get_games();
                return;
            }
            if(Png_offset[i]) {
                
                stops_BDVD = 1;
                Png_offset[12] = 0;

                wait_event_thread();
                int_currentdir = currentdir;
                // program new event thread function
                event_thread_send(0x555ULL, (u64) get_pict, (u64) &i); 
               
                currentgamedir = get_currentdir(i);
                if(currentgamedir >= 0 && currentgamedir < ndirectories) {
                    // access to PSX configuration menu
                    if(!mode_homebrew && 
                        (directories[(mode_favourites != 0) ? favourites.list[i].index : (currentdir + i)].flags & 
                        (1<<23)) == (1<<23)) // add PSX iso
                    {
                        if(!(directories[(mode_favourites != 0) ? favourites.list[i].index : (currentdir + i)].flags & (1<<11))) {
                            LoadPSXOptions(directories[(mode_favourites != 0) ? favourites.list[i].index : (currentdir + i)].path_name);
                        } else LoadPSXOptions(NULL);

                        if(get_psx_memcards()==0)
                            menu_screen = 444;
                    }
                    else {
                       menu_screen = 1;
                       load_gamecfg(-1); // force refresh game info
                       load_gamecfg (get_currentdir(i));
                       if(!(directories[currentgamedir].flags & (1<<11)) && lv2peek(0x80000000000004E8ULL))
                            set_BdId(currentgamedir);
                    }
                    return;
                }
            }
        }
    }

    static int auto_up = 0, auto_down = 0, auto_left = 0, auto_right = 0;

/************************************************************************************************************/
/* GUI 0                                                                                                    */
/************************************************************************************************************/

    if(gui_mode == 0) {

    AUTO_BUTTON_REP(auto_up, BUTTON_UP)
    AUTO_BUTTON_REP(auto_down, BUTTON_DOWN)
    AUTO_BUTTON_REP(auto_left, BUTTON_LEFT)
    AUTO_BUTTON_REP(auto_right, BUTTON_RIGHT)

    if(new_pad & BUTTON_UP) {
        select_py--;

        auto_up = 1;
        frame_count = 32;

        if(select_py < 0) {
            
            select_py = 2;
            
            if(mode_favourites >= 65536) ;
            else if(mode_favourites) {mode_favourites = 0; get_games();}
            else if(currentdir >= 12) {mode_favourites = 0; currentdir -= 12; get_games();}
            else {mode_favourites = (!mode_favourites && havefavourites); currentdir = ROUND_UP12(ndirectories) - 12; get_games();}
        }
        
        return;
    }

    if(new_pad & BUTTON_DOWN) {
        
        select_py++;

        auto_down = 1;
        frame_count = 32;
        
        if(select_py > 2) {
           
            select_py = 0; 
            
            if(mode_favourites >= 65536) ;
            else if(mode_favourites) {mode_favourites = 0; get_games();}
            else if(currentdir < (ROUND_UP12(ndirectories) - 12)) {mode_favourites = 0; currentdir += 12; get_games();}
            else {mode_favourites = (!mode_favourites && havefavourites); currentdir = 0; get_games();}
            
        }

        return;
    }

    if(new_pad & BUTTON_LEFT) {
        
        select_px--;

        auto_left = 1;
        frame_count = 32;

        if(select_px < 0) {
            
            select_px = 3;
            
            if(mode_favourites >= 65536) ;
            else if(mode_favourites) {mode_favourites = 0; get_games();}
            else if(currentdir >= 12) {mode_favourites = 0; currentdir -= 12; get_games();}
            else {mode_favourites = (!mode_favourites && havefavourites); currentdir = ROUND_UP12(ndirectories) - 12; get_games();}

        }

        return;
    }

    if(new_pad & BUTTON_RIGHT) {
        
        select_px++; 

        auto_right = 1;
        frame_count = 32;

        if(select_px > 3) {
            
            select_px = 0; 
            
            if(mode_favourites >= 65536) ;
            else if(mode_favourites) {mode_favourites = 0; get_games();}
            else if(currentdir < (ROUND_UP12(ndirectories) - 12)) {mode_favourites = 0; currentdir += 12; get_games();}
            else {mode_favourites = (!mode_favourites && havefavourites); currentdir = 0; get_games();}
        }

        return;
    }

    if(new_pad & BUTTON_L1) //change page
    {
        
        frame_count = 32;
            
        if(mode_favourites >= 65536) ;
        else if(mode_favourites) {mode_favourites = 0; get_games();}
        else if(currentdir >= 12) {mode_favourites = 0; currentdir -= 12; get_games();}
        else {mode_favourites = (!mode_favourites && havefavourites); currentdir = ROUND_UP12(ndirectories) - 12; get_games();}

        return;
    }
    
    if(new_pad & BUTTON_R1) //change page
    {
        //maybe wait some seconds here...
        frame_count = 32;
            
        if(mode_favourites >= 65536) ;
        else if(mode_favourites) {mode_favourites = 0; get_games();}
        else if(currentdir < (ROUND_UP12(ndirectories) - 12)) {mode_favourites = 0; currentdir += 12; get_games();}
        else {mode_favourites = (!mode_favourites && havefavourites); currentdir = 0; get_games();}

        return;
    }

// end of GUI 0
    } else {
/************************************************************************************************************/
/* GUI 1                                                                                                    */
/************************************************************************************************************/
    // AUTO_BUTTON_REP(auto_up, BUTTON_UP)
    //AUTO_BUTTON_REP(auto_down, BUTTON_DOWN)
    AUTO_BUTTON_REP3(auto_left, BUTTON_LEFT)
    AUTO_BUTTON_REP3(auto_right, BUTTON_RIGHT)

    if(new_pad & BUTTON_UP) {
        
        auto_up = 1;
        frame_count = 32;

        if(mode_favourites >= 65536) ;
        else if(mode_favourites) {
            mode_favourites = 0;
            currentdir = 0;
            select_px = select_py = 0;             
            get_games();

        } else {
            mode_favourites = (!mode_favourites && havefavourites);
            currentdir = 0;
            select_px = select_py = 0;
            get_games();
        }
        
        return;
    }

    if(new_pad & BUTTON_DOWN) {
        

        auto_down = 1;
        frame_count = 32;
        
        if(mode_favourites >= 65536) ;
        else if(mode_favourites) {
            mode_favourites = 0;
            currentdir = 0;
            select_px = select_py = 0;        
            get_games();

        } else {
            sort_mode++; if(sort_mode > 2) sort_mode = 0;

            sort_entries2(directories, &ndirectories, sort_mode);
            UpdateFavourites(directories, ndirectories);
            load_gamecfg(-1); // force refresh game info
            manager_cfg.gui_mode = ((sort_mode & 0xf)<<4) | (gui_mode & 0xf); 
            SaveManagerCfg();

            currentdir = 0;
            select_px = select_py = 0;
            get_games();
        }

        return;
    }

    if(new_pad & BUTTON_LEFT) {
        
        int tmp_currentdir = currentdir;
        int update = 0;

        select_px--;

        auto_left = 1;
        frame_count = 32;

        if(select_px < 0) {
            
            select_px = 3;

            select_py--;

            if(select_py < 0) {
                select_py = 2;
            
                if(mode_favourites >= 65536) ;
                else if(mode_favourites) {mode_favourites = 0; update = 1;
                        currentdir = ndirectories - 1;
                        if(currentdir < 0) currentdir = 0;
                        if(currentdir >= 3) {select_px = 3; currentdir-= 2;} else select_px = currentdir;
                    
                } else select_py = 5;
               
            }


        }

        if(!mode_favourites) {
        

            if(currentdir > 0 || (currentdir==0 && select_py != 5)) {
                if(currentdir > 0) {currentdir--; if(!update) update = 2; if(currentdir <= 3 && select_px == 2) select_px = 3;} 
                if(currentdir >= 3) select_px = 3; select_py = 0;}
            else {mode_favourites = (!mode_favourites && havefavourites);
                
                if(mode_favourites) {currentdir = (ndirectories/12) * 12; select_px = 3; select_py = 2;}
                else {
                    currentdir = ndirectories - 1;
                    if(currentdir < 0) currentdir = 0;
                    if(currentdir >= 3) {select_px = 3; currentdir-= 3;} else select_px = currentdir;
                    select_py = 0;
                }
            update = 1;
            }
            
        
        }

     
        if(currentdir < 0 || currentdir >= ndirectories)  {currentdir = 0; select_px = 0; select_py = 0; update = 1;}

        if(update)  {
            if(update==2 && currentdir == (tmp_currentdir - 1)) {

                wait_event_thread(); // wait previous event thread function

                int_currentdir = currentdir;

                // program new event thread function
                event_thread_send(0x555ULL, (u64) get_games_3, 0);

            } else get_games();
        }

        return;
    }

    if(new_pad & BUTTON_RIGHT) {
       
        int tmp_currentdir = currentdir;
        int update = 0;
        select_px++; 

        auto_right = 1;
        frame_count = 32;

        if(!mode_favourites && (currentdir + select_px) >= ndirectories) {currentdir = ndirectories; select_px = 4; select_py = 2;}

        if(select_px > 3) {
            
            select_px = 0; 

            select_py++;

            if(select_py > 2) {
                select_py = 0;

          
                if(mode_favourites >= 65536) ;
                else if(mode_favourites) {mode_favourites = 0; if(!update) update = 1; currentdir = 0; }
                else select_py = 1;
              
            }
            
        }

        //

        if(!mode_favourites) {
            if(select_px <= 3 && select_py == 0 && currentdir <= 3) ;
            else {

                if(currentdir < ndirectories) {currentdir++; update = 2;   select_px = 3; select_py = 0;}
                else {mode_favourites = (!mode_favourites && havefavourites); currentdir = 0; select_px = 0; select_py = 0;update = 1;}
            }
        
        }

     
        if(currentdir < 0 || currentdir >= ndirectories)  currentdir = 0; 

        if(update)  {
            if(update==2 && currentdir == (tmp_currentdir + 1)) {

                wait_event_thread(); // wait previous event thread function
            
                int_currentdir = currentdir;

                
                // program new event thread function
                event_thread_send(0x555ULL, (u64) get_games_3, 1ULL);

            } else get_games();
        }
        

        return;
    }

    if(new_pad & BUTTON_L1) //change page
    {
        
        frame_count = 32;
            
        if(mode_favourites >= 65536) ;
        else if(mode_favourites) {mode_favourites = 0; select_px = select_py = 0;get_games();}
        else if(currentdir >= 12) {mode_favourites = 0; currentdir -= 12; 
        if(currentdir < 0) currentdir = 0; if(currentdir >= 3) {select_px = 3; currentdir-= 3;} else select_px = currentdir; select_py = 0; select_py = 0;get_games();}
        else {mode_favourites = (!mode_favourites && havefavourites); currentdir = ndirectories - 12; 
        if(currentdir < 0) currentdir = 0; if(currentdir >= 3) {select_px = 3; currentdir-= 3;} else select_px = currentdir; select_py = 0; get_games();}

        return;
    }
    
    if(new_pad & BUTTON_R1) //change page
    {
        //maybe wait some seconds here...
        frame_count = 32;
            
        if(mode_favourites >= 65536) ;
        else if(mode_favourites) {if(currentdir >= ndirectories) {currentdir = 0; select_px = select_py = 0;} mode_favourites = 0; get_games();}
        else if(currentdir + 12 < ndirectories) {mode_favourites = 0; currentdir += 12; 
             select_px = select_py = 0; if(currentdir >= 3) {select_px = 3; currentdir-= 3;} else select_px = currentdir; get_games();}
        else {mode_favourites = (!mode_favourites && havefavourites); currentdir = 0; get_games();}



        return;
    }
// end of GUI 1
    }

    if(new_pad & BUTTON_R3) //change games/homebrew
    {

        select_px = select_py = 0;
        select_option = 0;
        menu_screen = 0;
        
        ndirectories = 0;

        fdevices=0;
        fdevices_old=0;
        forcedevices=0;
        find_device=0;
        bdvd_notify = 1;
        currentdir = 0;

        if(game_list_category == 0 && !mode_homebrew) {
            SetFavourites(mode_homebrew);
            mode_homebrew ^=HOMEBREW_MODE;
            GetFavourites(mode_homebrew);
            mode_favourites = 1; 
        } else if(mode_homebrew) {
            game_list_category=1;
            SetFavourites(mode_homebrew);
            mode_homebrew ^=HOMEBREW_MODE;
            GetFavourites(mode_homebrew);
            mode_favourites = 0;
        } else {
            mode_favourites = 0;
            game_list_category++; if(game_list_category > 2) {game_list_category = 0;mode_favourites = 1;}
        }
    }

    if(new_pad & BUTTON_L3) //change games/homebrew
    {

        select_px = select_py = 0;
        select_option = 0;
        menu_screen = 0;
        
        ndirectories = 0;

        fdevices=0;
        fdevices_old=0;
        forcedevices=0;
        find_device=0;
        bdvd_notify = 1;
        currentdir = 0;

        if(game_list_category == 1 && !mode_homebrew) {
            SetFavourites(mode_homebrew);
            mode_homebrew ^=HOMEBREW_MODE;
            GetFavourites(mode_homebrew);
            mode_favourites = 1; 
        } else if(mode_homebrew) {
            SetFavourites(mode_homebrew);
            mode_homebrew ^=HOMEBREW_MODE;
            GetFavourites(mode_homebrew);
            game_list_category=0;
            mode_favourites = 1;
        } else {
            mode_favourites = 0;
            game_list_category--; 
            if(game_list_category < 0) {game_list_category = 2;}
        }
    }

}



void draw_options(float x, float y, int index)
{

    int i, n;

    float y2, x2;

    int copy_flag = 1;

    int selected = select_px + select_py * 4;

    SetCurrentFont(FONT_TTF);

    // header title

    DrawBox(x, y, 0, 200 * 4 - 8, 20, 0x00000028);

    SetFontColor(0xffffffff, 0x00000000);

    SetFontSize(18, 20);

    SetFontAutoCenter(0);

  
    DrawFormatString(x, y, " %s", language[DRAWGMOPT_OPTS]);


    if(directories[currentgamedir].flags & 1) {

        copy_flag = 0;

        for(n = 1; n < 11; n++) {
            
            if((fdevices >> n) & 1) copy_flag = 1;
        }

    } else if(directories[currentgamedir].flags & 2046){
    
        copy_flag = 0;
        
        if((fdevices >> 0) & 1) copy_flag = 1; 
        
    }

    if(directories[currentgamedir].title_id[0] == 0 && select_option == 0) {
        select_option = 1;
        
        if(!copy_flag && select_option == 1) select_option++;

        if((directories[currentgamedir].flags & 2048) && (select_option == 2 || select_option == 6)) select_option++;

    }

    if(mode_homebrew == HOMEBREW_MODE) {
        if(select_option < 2) select_option= 2;
        if(select_option == 5) select_option= 6;
    }

    SetCurrentFont(FONT_BUTTON);
        
    SetFontSize(16, 20);

    utf8_to_ansi(directories[currentgamedir].title_id, temp_buffer, 64);
    temp_buffer[64] = 0;
    
    DrawFormatString(848 - x - strlen(temp_buffer) * 16 - 8, y, temp_buffer);

    y += 24;
    
    if(Png_offset[12]) {
               
        tiny3d_SetTextureWrap(0, Png_offset[12], Png_datas[12].width, 
        Png_datas[12].height, Png_datas[12].wpitch, 
            TINY3D_TEX_FORMAT_A8R8G8B8,  TEXTWRAP_CLAMP, TEXTWRAP_CLAMP,1);
        
    } else tiny3d_SetTextureWrap(0, Png_res_offset[16], Png_res[16].width, 
                    Png_res[16].height, Png_res[16].wpitch, 
                        TINY3D_TEX_FORMAT_A8R8G8B8,  TEXTWRAP_CLAMP, TEXTWRAP_CLAMP,1);
    
    DrawTextBox(x, y, 0, 200 * 4 - 8, 150 * 3 - 8, 0xffffffff);

    DrawBox(x, y, 0, 200 * 4 - 8, 150 * 3 - 8, 0x00000028);


    y2 = y + 8;
     
    if((directories[currentgamedir].flags & ((1<<11) | (1<<23))) == (1<<11)){
        n = sys_ss_media_id(BdId);
        if(n == 0 || n== 0x80010006) {
            SetFontSize(10, 16);
            SetFontColor(0x00afffff, 0x00000080);
            DrawFormatString(x + 32, y2, "BD ID: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                BdId[0], BdId[1], BdId[2], BdId[3], BdId[4], BdId[5], BdId[6], BdId[7], 
                BdId[8], BdId[9], BdId[10], BdId[11], BdId[12], BdId[13], BdId[14], BdId[15]);
        }
    } else if((BdId[0] | BdId[1] | BdId[2] | BdId[3] | BdId[4] | BdId[5] | BdId[6] | BdId[7] |
        BdId[8] | BdId[9] | BdId[10] | BdId[11] | BdId[12] | BdId[13] | BdId[14] | BdId[15])){
        SetFontSize(10, 16);
        SetFontColor(0xffaf00ff, 0x00000080);
        DrawFormatString(x + 32, y2, "BD ID: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
            BdId[0], BdId[1], BdId[2], BdId[3], BdId[4], BdId[5], BdId[6], BdId[7], 
            BdId[8], BdId[9], BdId[10], BdId[11], BdId[12], BdId[13], BdId[14], BdId[15]);

    }

    SetFontSize(12, 16);

    SetFontColor(0xffffffff, 0x00000000);

    x2 = x;
    y2 = y + 32;
    
    DrawButton1_UTF8(x + 32, y2, 320, language[DRAWGMOPT_CFGGAME], (directories[currentgamedir].title_id[0] == 0 ||
        mode_homebrew == HOMEBREW_MODE) ? -1 : (flash && select_option == 0));
    
    y2+= 48;

    DrawButton1_UTF8(x + 32, y2, 320, language[DRAWGMOPT_CPYGAME], (copy_flag!=0  && mode_homebrew != HOMEBREW_MODE) ? (flash && select_option == 1) : -1);
    
    y2+= 48;

    DrawButton1_UTF8(x + 32, y2, 320, language[DRAWGMOPT_DELGAME], (directories[currentgamedir].flags & 2048) ? -1  : ((flash && select_option == 2) ? 1 : 0));
    
    y2+= 48;

    DrawButton1_UTF8(x + 32, y2, 320, language[DRAWGMOPT_FIXGAME], (flash && select_option == 3));
    
    y2+= 48;

    DrawButton1_UTF8(x + 32, y2, 320, language[DRAWGMOPT_TSTGAME], (flash && select_option == 4));
    
    y2+= 48;

    DrawButton1_UTF8(x + 32, y2, 320, language[DRAWGMOPT_CPYEBOOTGAME], (directories[currentgamedir].title_id[0] == 0 || 
        mode_homebrew == HOMEBREW_MODE) ? -1 : (flash && select_option == 5));
    
    y2+= 48;
    
    if(!TestFavouritesExits(directories[currentgamedir].title_id))
        DrawButton1_UTF8(x + 32, y2, 320, language[DRAWGMOPT_CPYTOFAV], (directories[currentgamedir].flags & 2048) ? -1  : (flash && select_option == 6));
    else
        DrawButton1_UTF8(x + 32, y2, 320, language[DRAWGMOPT_DELFMFAV], (directories[currentgamedir].flags & 2048) ? -1  : (flash && select_option == 6));
    
    y2+= 48;

    DrawButton1_UTF8(x + 32, y2, 320, language[GLOBAL_RETURN], (flash && select_option == 7));
    
    y2+= 48;
    /*
    for(n = 0; n < 1; n++) {
        
        DrawButton1_UTF8(x + 32, y2, 320, "", -1);
    
        y2+= 48;
    }
    */

    SetCurrentFont(FONT_TTF);

    // draw game name

    DrawBox(x, y + 3 * 150, 0, 200 * 4 - 8, 40, 0x00000028);

    SetFontColor(0xffffffff, 0x00000000);

    i = selected;

    if(Png_offset[i]) {

        
        u32 str_color = 0xffffffff;

        if((directories[currentgamedir].flags  & GAMELIST_FILTER)== (1<<11)) {
            if(strncmp((char *) string_title_utf8, bluray_game, 64)) {
                strncpy((char *) string_title_utf8, bluray_game, 128);
                update_title_utf8 = 1;
            }
            str_color = 0x00ff00ff;
        } else {
            if(strncmp((char *) string_title_utf8, directories[currentgamedir].title, 64)) {
                strncpy((char *) string_title_utf8, directories[currentgamedir].title, 128);
                update_title_utf8 = 1;
            }
        }

        
        if(update_title_utf8) {
            width_title_utf8 =  Render_String_UTF8(ttf_texture, 768, 32, string_title_utf8, 16, 24);
            update_title_utf8 = 0;
        }
         
       
        tiny3d_SetTextureWrap(0, tiny3d_TextureOffset(ttf_texture), 768, 
                32, 768 * 2, 
                TINY3D_TEX_FORMAT_A4R4G4B4,  TEXTWRAP_CLAMP, TEXTWRAP_CLAMP,1);
        DrawTextBox((848 - width_title_utf8) / 2, y + 3 * 150 , 0, 768, 32, str_color);
    
    }
    
    tiny3d_Flip();

    ps3pad_read();
   
    if(new_pad & (BUTTON_CROSS | BUTTON_CIRCLE)) {

        switch(select_option) {
            case 0:
                if(mode_homebrew == HOMEBREW_MODE) break;
                select_option = 0;
                menu_screen = 2; 

                // load game config
                sprintf(temp_buffer, "%s/config/%s.cfg", self_path, directories[currentgamedir].title_id);
                memset(&game_cfg, 0, sizeof(game_cfg));
                
                int file_size;
                char *file = LoadFile(temp_buffer, &file_size);
                if(file) {
                    if(file_size > sizeof(game_cfg)) file_size = sizeof(game_cfg);
                    memcpy(&game_cfg, file, file_size);
                    free(file);
                }
                return;

            case 1:
                if(mode_homebrew == HOMEBREW_MODE) break;
                if(test_ftp_working()) break;
                 i = selected;

                 if(Png_offset[i]) {

                    pause_music(1);

                    copy_from_selection(currentgamedir);

                    pause_music(0);

                    currentgamedir = currentdir = 0;
                    select_px = select_py = 0;
                    select_option = 0;
                    menu_screen = 0;
                    stops_BDVD = 1;
                 }
                 return;

            case 2:
                 i = selected;
                 if(test_ftp_working()) break;

                 if(Png_offset[i]) {
                    
                    pause_music(1);
                    
                    delete_game(currentgamedir);
                    
                    pause_music(0);

                    currentgamedir = currentdir = 0;
                    select_px = select_py = 0;
                    select_option = 0;
                    menu_screen = 0;
                    stops_BDVD = 1;
                 }
                 return;
            case 3:
                 i = selected;
                 
                 if(Png_offset[i]) {

                    pause_music(1);                

                    // sys8_perm_mode(1);
                    FixDirectory(directories[currentgamedir].path_name);
                    // sys8_perm_mode(0);
                    
                    pause_music(0);

                    DrawDialogOK(language[DRAWGMOPT_FIXCOMPLETE]);
                    stops_BDVD = 1;
             
                 }
                 break;
            case 4:
                 i = selected;

                 if(Png_offset[i]) {

                    pause_music(1);

                    test_game(currentgamedir);

                    pause_music(0);
                    stops_BDVD = 1;
                    
                 }
                 break;
            case 5:
                {
                if(mode_homebrew == HOMEBREW_MODE) break;
                // load game config
                sprintf(temp_buffer, "/dev_usb/ps3game/%s.BIN", directories[currentgamedir].title_id);
                
                int file_size;
                char *file = LoadFile(temp_buffer, &file_size);
                if(file) {
                    sprintf(temp_buffer, "%s/self/%s.BIN", self_path, directories[currentgamedir].title_id);
                    if(SaveFile(temp_buffer, file, file_size)==0) {
                        sprintf(temp_buffer, "%s/self/%s.BIN\n\nEBOOT.BIN %s", self_path, directories[currentgamedir].title_id, language[DRAWGMOPT_CPYOK]);
                        DrawDialogOK(temp_buffer);
                    } else {
                        sprintf(temp_buffer, "%s/self/%s.BIN\n\n%s EBOOT.BIN", self_path, directories[currentgamedir].title_id, language[DRAWGMOPT_CPYERR]);
                        DrawDialogOK(temp_buffer);
                    }
                    free(file);
                } else {
                    sprintf(temp_buffer, "/dev_usb/ps3game/%s.BIN\n\nEBOOT.BIN %s", directories[currentgamedir].title_id, language[DRAWGMOPT_CPYNOTFND]);
                    DrawDialogOK(temp_buffer);
                }

                }
                break;
            
            case 6:
                if(TestFavouritesExits(directories[currentgamedir].title_id)) 
                    {
                        DeleteFavouritesIfExits(directories[currentgamedir].title_id);

                        sprintf(temp_buffer, "%s/config/", self_path);
                        SaveFavourites(temp_buffer, mode_homebrew);

                        if(mode_favourites && !havefavourites) {
                            mode_favourites = 0; get_games(); select_option = 0;
                            menu_screen = 0;
                            return;
                        }
                    
                        get_games();
                    }
                else {
                    mode_favourites = currentgamedir  | 65536;
                    
                    get_icon(path_name, currentgamedir);
                    if(!strncmp(path_name + strlen(path_name) -4, ".JPG", 4) || !strncmp(path_name + strlen(path_name) -4, ".jpg", 4))
                        LoadTextureJPG(path_name, 12);
                        else
                            if(LoadTexturePNG(path_name, 12) < 0) ;
                    get_games();
                    select_option = 0;
                    menu_screen = 0;
                    return;
                    
                }
                break;
            case 7:
                Png_offset[12] = 0;
                select_option = 0;
                menu_screen = 0;
                return;

            default:
               break;
        }
       // menu_screen = 0; return;
    }

    if(new_pad & BUTTON_TRIANGLE) {
        Png_offset[12] = 0; menu_screen = 0; select_option = 0; return;
    }
   

    if(new_pad & BUTTON_UP) {
        select_option--; 

        frame_count = 32;

        if((directories[currentgamedir].flags & 2048) && (select_option == 2 || select_option == 6)) select_option--;
        if(!copy_flag && select_option == 1) select_option--;

        if(directories[currentgamedir].title_id[0] == 0 && (select_option == 0 || select_option == 5)) select_option--;

        if(mode_homebrew == HOMEBREW_MODE) {
            if(select_option < 2) select_option= -1;
            if(select_option == 5) select_option= 4;
        }

        if(select_option < 0) {
            
            select_option = 7;  
          
        }
    }

    if(new_pad & BUTTON_DOWN) {
        
        select_option++;

        frame_count = 32;

        if(!copy_flag && select_option == 1) select_option++;

        if((directories[currentgamedir].flags & 2048) &&  (select_option == 2 || select_option == 6)) select_option++;

        if(mode_homebrew == HOMEBREW_MODE) {
            if(select_option < 2) select_option = 2;
            if(select_option == 5) select_option = 6;
        }
        
        if(select_option > 7) {
           
            select_option = 0;
           
        }

        if(directories[currentgamedir].title_id[0] == 0 && (select_option == 0 || select_option == 5)) {
            select_option++;
            
            if(!copy_flag && select_option == 1) select_option++;

            if((directories[currentgamedir].flags & 2048) && select_option == 2) select_option++; 

        }
    }

    
}


void draw_configs(float x, float y, int index)
{

    int i;

    float y2, x2;
   
    int selected = select_px + select_py * 4;

    SetCurrentFont(FONT_TTF);

    // header title

    DrawBox(x, y, 0, 200 * 4 - 8, 20, 0x00000028);

    SetFontColor(0xffffffff, 0x00000000);

    SetFontSize(18, 20);

    SetFontAutoCenter(0);
  
    DrawFormatString(x, y, " %s", language[DRAWGMCFG_CFGS]);

    i = selected;

    SetCurrentFont(FONT_BUTTON);
        
    SetFontSize(16, 20);

    utf8_to_ansi(directories[currentgamedir].title_id, temp_buffer, 64);
    temp_buffer[64] = 0;
    
    DrawFormatString(848 - x - strlen(temp_buffer) * 16 - 8, y, temp_buffer);

    y += 24;
    

    if(Png_offset[12]) {
               
        tiny3d_SetTextureWrap(0, Png_offset[12], Png_datas[12].width, 
        Png_datas[12].height, Png_datas[12].wpitch, 
            TINY3D_TEX_FORMAT_A8R8G8B8,  TEXTWRAP_CLAMP, TEXTWRAP_CLAMP,1);
        
    } else tiny3d_SetTextureWrap(0, Png_res_offset[16], Png_res[16].width, 
                    Png_res[16].height, Png_res[16].wpitch, 
                        TINY3D_TEX_FORMAT_A8R8G8B8,  TEXTWRAP_CLAMP, TEXTWRAP_CLAMP,1);
    
    DrawTextBox(x, y, 0, 200 * 4 - 8, 150 * 3 - 8, 0xffffffff);

    DrawBox(x, y, 0, 200 * 4 - 8, 150 * 3 - 8, 0x00000028);

    
    x2 = x;
    y2 = y + 32;


#ifdef CONFIG_USE_SYS8PERMH4
    x2 = DrawButton1_UTF8(x + 32, y2, 320, "Fix Permissions", (flash && select_option == 0)) + 16; // do no translate this (3.44)
    
    x2 = DrawButton2_UTF8(x2, y2, 0, " Default ", (game_cfg.perm == 0) ) + 8;
    x2 = DrawButton2_UTF8(x2, y2, 0, " PS jailbreak ", (game_cfg.perm == 1)) + 8;
    x2 = DrawButton2_UTF8(x2, y2, 0, " v4 Perms (F1) ", (game_cfg.perm == 2)) + 8;

    y2+= 48;
#endif



    x2 = DrawButton1_UTF8(x + 32, y2, 320, language[DRAWGMCFG_DSK], (flash && select_option == 0))  + 16;
    x2 = DrawButton2_UTF8(x2, y2, 0, language[DRAWGMCFG_NO] , (game_cfg.useBDVD == 0)) + 8;
    x2 = DrawButton2_UTF8(x2, y2, 0, language[DRAWGMCFG_YES], (game_cfg.useBDVD == 1)) + 8;

    y2+= 48;

    x2 = DrawButton1_UTF8(x + 32, y2, 320, "Direct Boot", (flash && select_option == 1)) + 16;
    
    x2 = DrawButton2_UTF8(x2, y2, 0, language[DRAWGMCFG_NO], (game_cfg.direct_boot == 0) ) + 8;
    x2 = DrawButton2_UTF8(x2, y2, 0, language[DRAWGMCFG_YES], (game_cfg.direct_boot == 1)) + 8;
    x2 = DrawButton2_UTF8(x2, y2, 0, "With BR", (game_cfg.direct_boot == 2)) + 8;

    y2+= 48;

    x2 = DrawButton1_UTF8(x + 32, y2, 320, language[DRAWGMCFG_EXTBOOT], (flash && select_option == 2))  + 16;
        
    x2 = DrawButton2_UTF8(x2, y2, 0, language[DRAWGMCFG_ON] , (payload_mode >= ZERO_PAYLOAD) ? (game_cfg.ext_ebootbin != 0) : -1 ) + 8;
    x2 = DrawButton2_UTF8(x2, y2, 0, language[DRAWGMCFG_OFF], (game_cfg.ext_ebootbin == 0)) + 8;

    y2+= 48;

    x2 = DrawButton1_UTF8(x + 32, y2, 320, language[DRAWGMCFG_BDEMU], (flash && select_option == 3))  + 16;
    
    if(directories[currentgamedir].flags & 1) {
        x2 = DrawButton2_UTF8(x2, y2, 0, "Mount BDVD" ,(game_cfg.bdemu == 1)) + 8;
        x2 = DrawButton2_UTF8(x2, y2, 0, "LIBFS" , (game_cfg.bdemu > 1)) + 8;
        x2 = DrawButton2_UTF8(x2, y2, 0, language[DRAWGMCFG_OFF], (game_cfg.bdemu == 0)) + 8;
    } else {
        x2 = DrawButton2_UTF8(x2, y2, 0, "Mount BDVD" ,(game_cfg.bdemu_ext == 1)) + 8;
        x2 = DrawButton2_UTF8(x2, y2, 0, "LIBFS" , (game_cfg.bdemu_ext > 1)) + 8;
        x2 = DrawButton2_UTF8(x2, y2, 0, language[DRAWGMCFG_OFF], (game_cfg.bdemu_ext == 0)) + 8;
    }

    y2+= 48;

    x2 = DrawButton1_UTF8(x + 32, y2, 320, language[DRAWGMCFG_EXTHDD0GAME], (flash && select_option == 4))  + 16;
        
    x2 = DrawButton2_UTF8(x2, y2, 0, language[DRAWGMCFG_ON] , (payload_mode >= ZERO_PAYLOAD) ? (game_cfg.exthdd0emu != 0): -1) + 8;
    x2 = DrawButton2_UTF8(x2, y2, 0, language[DRAWGMCFG_OFF], (game_cfg.exthdd0emu == 0)) + 8;

    y2+= 48;

    x2 = DrawButton1_UTF8(x + 32, y2, 320, language[DRAWGMCFG_SAVECFG], (flash && select_option == 5))  + 16;
    y2+= 48;

    x2 = DrawButton1_UTF8(x + 32, y2, 320, language[GLOBAL_RETURN], (flash && select_option == 6))  + 16;
    y2+= 48;


    SetCurrentFont(FONT_TTF);

    // draw game name

    i = selected;

    DrawBox(x, y + 3 * 150, 0, 200 * 4 - 8, 40, 0x00000028);

    SetFontColor(0xffffffff, 0x00000000);

    if(Png_offset[i]) {

        u32 str_color = 0xffffffff;

        if((directories[currentgamedir].flags  & GAMELIST_FILTER)== (1<<11)) {
            if(strncmp((char *) string_title_utf8, bluray_game, 64)) {
                strncpy((char *) string_title_utf8, bluray_game, 128);
                update_title_utf8 = 1;
            }
            str_color = 0x00ff00ff;
        } else {
            if(strncmp((char *) string_title_utf8, directories[currentgamedir].title, 64)) {
                strncpy((char *) string_title_utf8, directories[currentgamedir].title, 128);
                update_title_utf8 = 1;
            }
        }

        if(update_title_utf8) {
            width_title_utf8 =  Render_String_UTF8(ttf_texture, 768, 32, string_title_utf8, 16, 24);
            update_title_utf8 = 0;
        }
         
       
        tiny3d_SetTextureWrap(0, tiny3d_TextureOffset(ttf_texture), 768, 
                32, 768 * 2, 
                TINY3D_TEX_FORMAT_A4R4G4B4,  TEXTWRAP_CLAMP, TEXTWRAP_CLAMP,1);
        DrawTextBox((848 - width_title_utf8) / 2, y + 3 * 150 , 0, 768, 32, str_color);
    }
   

    tiny3d_Flip();

    ps3pad_read();


    if(new_pad & BUTTON_TRIANGLE) {
        menu_screen = 1; select_option = 0; return;
    }

   
    if(new_pad & (BUTTON_CROSS | BUTTON_CIRCLE)) {
     
        switch(select_option) {
            //removed sys8 calls not supported yet on 3.55
#ifdef CONFIG_USE_SYS8PERMH4
            case 0:
                ROT_INC(game_cfg.perm, 2, 0);
                break;
#endif
            case 0:
                ROT_INC(game_cfg.useBDVD, 1, 0);
                break;
            case 1:
                ROT_INC(game_cfg.direct_boot, 2, 0);
                break;
            case 2:
                if(payload_mode >= ZERO_PAYLOAD)
                    ROT_INC(game_cfg.ext_ebootbin, 1, 0);
                break;
            case 3:
                if(directories[currentgamedir].flags & 1)
                    ROT_INC(game_cfg.bdemu, 2, 0)
                else
                   ROT_INC(game_cfg.bdemu_ext, 2, 0) 
                break;
            case 4:
                if(payload_mode >= ZERO_PAYLOAD)
                    ROT_INC(game_cfg.exthdd0emu, 1, 0);
                break;
            case 5:
                // save game config
                sprintf(temp_buffer, "%s/config/%s.cfg", self_path, directories[currentgamedir].title_id);
              
                
                if(SaveFile(temp_buffer, (char *) &game_cfg, sizeof(game_cfg)) == 0) {
                    sprintf(temp_buffer, "%s.cfg\n\n%s", directories[currentgamedir].title_id, language[GLOBAL_SAVED]);
                    DrawDialogOK(temp_buffer);
                }

                break;
            default:
                menu_screen = 1; select_option = 0; return;
                break;
        }
     }

    if(new_pad & BUTTON_UP) {

        frame_count = 32;

        ROT_DEC(select_option, 0, 6)
        
    }

    if(new_pad & BUTTON_DOWN) {
        
        frame_count = 32;

        ROT_INC(select_option, 6, 0); 
        
    }

    if(new_pad & BUTTON_LEFT) {
     
        switch(select_option) {
            //removed sys8 calls not supported yet on 3.55
#ifdef CONFIG_USE_SYS8PERMH4
            case 0:
                ROT_DEC(game_cfg.perm, 0, 2);
                break;
#endif
            case 0:
                ROT_DEC(game_cfg.useBDVD, 0, 1);
                break;
            case 1:
                ROT_DEC(game_cfg.direct_boot, 0, 2);
                break;
            case 2:
                if(payload_mode >= ZERO_PAYLOAD)
                    ROT_DEC(game_cfg.ext_ebootbin, 0, 1);
                break;
            case 3:
                if(directories[currentgamedir].flags & 1)
                    ROT_DEC(game_cfg.bdemu, 0, 2)
                else
                    ROT_DEC(game_cfg.bdemu_ext, 0, 2)
                break;
             case 4:
                if(payload_mode >= ZERO_PAYLOAD)
                    ROT_DEC(game_cfg.exthdd0emu, 0, 1);
                break;
            default:
                break;
        }
     }

     if(new_pad & BUTTON_RIGHT) {
     
        switch(select_option) {
            //removed sys8 calls not supported yet on 3.55
#ifdef CONFIG_USE_SYS8PERMH4
            case 0:
                ROT_INC(game_cfg.perm, 2, 0);
                break;
#endif
            case 0:
                ROT_INC(game_cfg.useBDVD, 1, 0);
                break;
            case 1:
                ROT_INC(game_cfg.direct_boot, 2, 0);
                break;
            case 2:
                if(payload_mode >= ZERO_PAYLOAD)
                    ROT_INC(game_cfg.ext_ebootbin, 1, 0);
                break;
            case 3:
                if(directories[currentgamedir].flags & 1)
                    ROT_INC(game_cfg.bdemu, 2, 0)
                else
                    ROT_INC(game_cfg.bdemu_ext, 2, 0)
                break;
            case 4:
                if(payload_mode >= ZERO_PAYLOAD)
                    ROT_INC(game_cfg.exthdd0emu, 1, 0);
                break;
            default:
                break;
        }
     }

}

static char help1[]= {
    "Test of Character Set\n"
    "Hola Hello Bonjour Ciao Hallo مرحبا\n"
    "привет Γεια σας もしもし Merhaba ğ\n"
    "안녕하세요. 你好\n"
    "Dışişleri Bakanlığı\n"
    "Мне нужно немного водки\n"
    "我想貴國訪問\n"
    "Γαμημένοι πολιτικοί που έχουν προκαλέσει αναστάτωση\n"
    "スペインからのご挨拶\n"
    "تحيات من اسبانيا\n"
};

void draw_gbloptions(float x, float y)
{

    float y2, x2;
    static float x3 = -1;
    static int help = 0;
    
    SetCurrentFont(FONT_TTF);

    // header title

    DrawBox(x, y, 0, 200 * 4 - 8, 20, 0x00000028);

    SetFontColor(0xffffffff, 0x00000000);

    SetFontSize(18, 20);

    SetFontAutoCenter(0);
  
    DrawFormatString(x, y, " %s", language[DRAWGLOPT_OPTS]);

    if(x3 < 0)
    {
        x3 = 2000;
        x3 = DrawFormatString(x3, y,  music[song_selected + MAX_SONGS]); // calculate first time
        x3 = 848 -(x3 - 2000) - x;
    }else
        DrawFormatString(x3, y,  music[song_selected + MAX_SONGS]); //print current song name
    
    y += 24;

    DrawBox(x, y, 0, 200 * 4 - 8, 150 * 3 - 8, 0x00000028);

    
    x2 = x;
    y2 = y + 32 - 24;
    
    DrawButton1_UTF8((848 - 520) / 2, y2, 520, language[DRAWGLOPT_SCRADJUST], (flash && select_option == 0));
    
    y2+= 48;

    DrawButton1_UTF8((848 - 520) / 2, y2, 520, language[DRAWGLOPT_CHANGEGUI], (flash && select_option == 1));
    
    y2+= 48;

    DrawButton1_UTF8((848 - 520) / 2, y2, 520, language[DRAWGLOPT_CHANGEBCK], (flash && select_option == 2));
    
    y2+= 48;

    DrawButton1_UTF8((848 - 520) / 2, y2, 520, language[DRAWGLOPT_CHANGEDIR], (flash && select_option == 3));
    
    y2+= 48;

    DrawButton1_UTF8((848 - 520) / 2, y2, 520, (manager_cfg.opt_flags & OPTFLAGS_PLAYMUSIC)? language[DRAWGLOPT_SWMUSICOFF] : language[DRAWGLOPT_SWMUSICON] , (flash && select_option == 4));
    
    y2+= 48;

    DrawButton1_UTF8((848 - 520) / 2, y2, 520, (ftp_ip_str[0]) ? ftp_ip_str : language[DRAWGLOPT_INITFTP], (flash && select_option == 5));
    
    y2+= 48;

    DrawButton1_UTF8((848 - 520) / 2, y2, 520, language[DRAWGLOPT_TOOLS], (flash && select_option == 6));
    
    y2+= 48;

    DrawButton1_UTF8((848 - 520) / 2, y2, 520, language[GLOBAL_RETURN], (flash && select_option == 7));
    
    y2+= 48;
    
    DrawButton1_UTF8((848 - 520) / 2, y2, 520, language[DRAWGLOPT_CREDITS], (flash && select_option == 8));
   
    y2+= 48;

    DrawBox(x, y + 3 * 150, 0, 200 * 4 - 8, 40, 0x00000028);

    // draw sys version
    SetCurrentFont(FONT_TTF);

    SetFontColor(0xccccffff, 0x00000000);
    SetFontSize(18, 20);
    SetFontAutoCenter(1);
    DrawFormatString(0, y2 + 40 - 24, payload_str );
    SetFontAutoCenter(0);
    
    if(help) {
        
        DrawBox((848 - 624)/2, (512 - 424)/2, 0, 624, 424, 0x602060ff);
        DrawBox((848 - 616)/2, (512 - 416)/2, 0, 616, 416, 0x802080ff);
        set_ttf_window((848 - 600)/2, (512 - 416)/2, 600, 416, WIN_AUTO_LF);

        display_ttf_string(0, 0, help1, 0xffffffff, 0, 18, 24);

        SetFontAutoCenter(1);
        
        if(lv2peek(0x80000000000004E8ULL)) ;
            DrawFormatString(0, (512 - 416)/2 - 20, "Event ID: %x / VSH ID %x", (u32) syscall_40(4, 0), manager_cfg.event_flag);
        SetFontAutoCenter(0);

        if(lv2peek(0x80000000000004E8ULL)) {
            u32 eid= (u32) syscall_40(4, 0);

            if(eid!=0) {
                eid -= 0x100;
                if(eid != manager_cfg.event_flag) {
                    manager_cfg.event_flag = eid;
                    SaveManagerCfg();
                }
            }
        }
    }

    tiny3d_Flip();

    ps3pad_read();

    if(new_pad & BUTTON_SELECT) {
        help^=1;
    }

    if(new_pad & BUTTON_TRIANGLE) {
        help = 0;
        menu_screen = 0; return;
    }

    if(help) return;

    if(new_pad & (BUTTON_CROSS | BUTTON_CIRCLE)) {
    
        switch(select_option) {
            case 0:
                video_adjust();
                select_option = 0;
                menu_screen = 0; 
                return;

            case 1:
                gui_mode = gui_mode==0;
                manager_cfg.gui_mode = ((sort_mode & 0xf)<<4) | (gui_mode & 0xf);
                select_option = 0;
                menu_screen = 0; 
                SaveManagerCfg();
                currentgamedir = currentdir = 0;
                select_px = select_py = 0;
                select_option = 0;
                menu_screen = 0;
                get_games();
                load_gamecfg(-1); // force refresh game info

                return;

            case 2:
                background_sel++;
                background_sel &= 7;
                manager_cfg.background_sel = background_sel;
                SaveManagerCfg();
                break;

            case 3:
                menu_screen = 0;
                Select_games_folder();
     
                if(manager_cfg.hdd_folder[0] == 0) strcpy(manager_cfg.hdd_folder, __MKDEF_MANAGER_DIR__);
                SaveManagerCfg();
                currentgamedir = currentdir = 0;
                select_px = select_py = 0;
                select_option = 0;
                menu_screen = 0;
                
                ndirectories = 0;
                fdevices=0;
                fdevices_old=0;
                forcedevices=0;
                find_device=0;
                bdvd_notify = 1;
                load_gamecfg(-1); // force refresh game info

                return;

            case 4:
                manager_cfg.opt_flags ^= OPTFLAGS_PLAYMUSIC; //change bit
                pause_music((manager_cfg.opt_flags & OPTFLAGS_PLAYMUSIC)? 0 : 1);
                SaveManagerCfg();
                break;

            case 5:
                if(test_ftp_working()) break;
                if ((manager_cfg.opt_flags & OPTFLAGS_FTP) == 0)
                {
                    int r = ftp_init();
                    if(r == 0)
                    {
                        ftp_inited = 1;
                        if(DrawDialogYesNo(language[DRAWGLOPT_FTPINITED]) != 1)
                            break;
                    }
                    else
                    {
                        if(r == -1) DrawDialogOK("Error in netInitialize()");
                        else if(r == -2) DrawDialogOK("Error in netCtlInit()");
                        else if(r == -3) DrawDialogOK("Error in netCtlGetInfo()");
                        else if(r == -4) DrawDialogOK("Net Disconnected or Connection not Established");
                        else DrawDialogOK(language[DRAWGLOPT_FTPARINITED]);

                        break;
                    }
                }
                else
                {
                        DrawDialogOK(language[DRAWGLOPT_FTPSTOPED]);
                        ftp_deinit();
                        ftp_inited = 0;
                }
                manager_cfg.opt_flags ^= OPTFLAGS_FTP;
                SaveManagerCfg();
                break;

            case 6:
                select_option = 0;
                menu_screen = 4; 
                return;

            case 7:
                select_option = 0;
                menu_screen = 0; 
                return;

            case 8:
                   DrawDialogOK(credits_str1);
                   DrawDialogOK(credits_str2);
                   DrawDialogOK(credits_str3);
                   break;

            default:
               break;
        }
    
    }


    if(new_pad & BUTTON_UP) {

        frame_count = 32;

        ROT_DEC(select_option, 0, 8)
        
    }

    if(new_pad & BUTTON_DOWN) {

        frame_count = 32;
        
        ROT_INC(select_option, 8, 0); 
        
    }
    
}

void draw_toolsoptions(float x, float y)
{

    int n;

    float y2, x2;

    SetCurrentFont(FONT_TTF);

    // header title

    DrawBox(x, y, 0, 200 * 4 - 8, 20, 0x00000028);

    SetFontColor(0xffffffff, 0x00000000);

    SetFontSize(18, 20);

    SetFontAutoCenter(0);
  
    DrawFormatString(x, y, " %s", language[DRAWTOOLS_TOOLS]);

    y += 24;

    DrawBox(x, y, 0, 200 * 4 - 8, 150 * 3 - 8, 0x00000028);


    x2 = x;
    y2 = y + 32;
    
    DrawButton1_UTF8((848 - 520) / 2, y2, 520, language[DRAWTOOLS_DELCACHE], (flash && select_option == 0));
    
    y2+= 48;
/*
    if(manager_cfg.usekey)
        DrawButton1_UTF8((848 - 520) / 2, y2, 520, language[DRAWTOOLS_SECDISABLE], (flash && select_option == 1));
    else
        DrawButton1_UTF8((848 - 520) / 2, y2, 520, language[DRAWTOOLS_SECENABLE], (flash && select_option == 1));
*/
    DrawButton1_UTF8((848 - 520) / 2, y2, 520, language[DRAWTOOLS_LANGUAGE_1 + (manager_cfg.language & 15)], (flash && select_option == 1));
    
    y2+= 48;

    DrawButton1_UTF8((848 - 520) / 2, y2, 520, language[DRAWTOOLS_PKGTOOLS], (flash && select_option == 2));
    
    y2+= 48;

    DrawButton1_UTF8((848 - 520) / 2, y2, 520, language[DRAWTOOLS_COPYFROM], (flash && select_option == 3));

    y2+= 48;

    if(!manager_cfg.noBDVD)
        DrawButton1_UTF8((848 - 520) / 2, y2, 520, language[DRAWTOOLS_WITHBDVD], (flash && select_option == 4));
    else if(manager_cfg.noBDVD == 2)
        DrawButton1_UTF8((848 - 520) / 2, y2, 520, language[DRAWTOOLS_NOBDVD2], (flash && select_option == 4));
    else
        DrawButton1_UTF8((848 - 520) / 2, y2, 520, language[DRAWTOOLS_NOBDVD], (flash && select_option == 4));

    y2+= 48;

    DrawButton1_UTF8((848 - 520) / 2, y2, 520, language[DRAWTOOLS_ARCHIVEMAN], (flash && select_option == 5));

    y2+= 48;

    DrawButton1_UTF8((848 - 520) / 2, y2, 520, "Control Fan & USB Wakeup"/*language[DRAWTOOLS_ARCHIVEMAN]*/, (flash && select_option == 6));

    y2+= 48;

    DrawButton1_UTF8((848 - 520) / 2, y2, 520, language[GLOBAL_RETURN], (flash && select_option == 7));
    
    y2+= 48;
    
    for(n = 0; n < 1; n++) {
        
        DrawButton1_UTF8((848 - 520) / 2, y2, 520, "", -1);
    
        y2+= 48;
    }


    SetCurrentFont(FONT_TTF);

    // draw game name

    DrawBox(x, y + 3 * 150, 0, 200 * 4 - 8, 40, 0x00000028);

    SetFontColor(0xffffffff, 0x00000000);


    tiny3d_Flip();

    ps3pad_read();

    if(new_pad & (BUTTON_CROSS | BUTTON_CIRCLE)) {
    
        switch(select_option) {
            case 0:
               
                LoadCacheDatas();
                // draw_cachesel

                if(ncache_list > 0) {
                    menu_screen = 5;
                    select_option = 0;
                }

                return;
            case 1:
                manager_cfg.language++; if(manager_cfg.language > 10)  manager_cfg.language = 0;
                sprintf(temp_buffer, "%s/config/language.ini", self_path);
                open_language(manager_cfg.language, temp_buffer);
                //manager_cfg.usekey = manager_cfg.usekey == 0;
                SaveManagerCfg();
                
                break;

            case 2:
                draw_pkginstall(x, y);
                break;

            case 3:
                if(test_ftp_working()) break;
                copy_usb_to_iris(self_path);
                break;

            case 4:
                if(test_ftp_working()) break;
                manager_cfg.noBDVD = 0; //(manager_cfg.noBDVD ==0) & 1;

                sprintf(temp_buffer, "%s\n\n%s?", language[GAMEFOLDER_WANTUSE], language[DRAWTOOLS_NOBDVD2]);
                
                if(DrawDialogYesNo(temp_buffer) == 1) {
                    manager_cfg.noBDVD = 2;
                } else {

                    sprintf(temp_buffer, "%s\n\n%s?", language[GAMEFOLDER_WANTUSE], language[DRAWTOOLS_NOBDVD]);

                    if(DrawDialogYesNo(temp_buffer) == 1) {
                        manager_cfg.noBDVD = 1;
                    }
                }


                noBDVD = manager_cfg.noBDVD & 3;
                SaveManagerCfg();
                sys_fs_umount("/dev_bdvd");
                sys_fs_umount("/dev_ps2disc");
                if(noBDVD) Eject_BDVD(NOWAIT_BDVD | EJECT_BDVD);
                
                if(!manager_cfg.noBDVD)
                      sprintf(temp_buffer, "%s\n\n%s\n\n%s?", language[DRAWTOOLS_WITHBDVD],
                            "Exiting to the XMB: launch Iris Manager again",
                            "Saliendo al XMB: lanza Iris Manager de nuevo");
                else if(manager_cfg.noBDVD == 2)
                      sprintf(temp_buffer, "%s\n\n%s\n\n%s?", language[DRAWTOOLS_NOBDVD2],
                            "Exiting to the XMB: launch Iris Manager again",
                            "Saliendo al XMB: lanza Iris Manager de nuevo");
                else 
                      sprintf(temp_buffer, "%s\n\n%s\n\n%s?", language[DRAWTOOLS_NOBDVD],
                            "Exiting to the XMB: launch Iris Manager again",
                            "Saliendo al XMB: lanza Iris Manager de nuevo");

                DrawDialogOKTimer(temp_buffer, 2000.0f);
                game_cfg.direct_boot=0;
                exit(0);
                break;
				
            case 5: 
				if( firmware < 0x446C && firmware >= 0x421C ) //Fix psn/update
				{
					pause_music( 1 );
					DrawDialogTimer( language[DRAWTOOLS_FIXFWVERPSNUPDWAIT], 1000.0f );
					patch_error_09( "/dev_hdd0/game" );
					pause_music( 0 );
					DrawDialogOK( language[DRAWTOOLS_FIXFWVERPSNUPDOK] );
					break;
				}
				else
				{
				
                archive_manager();

                select_px = select_py = 0;
                select_option = 0;
                menu_screen = 0;
                
                ndirectories = 0;

                fdevices=0;
                fdevices_old=0;
                forcedevices=0;
                find_device=0;
                bdvd_notify = 1;
                currentdir = 0;

                select_option = 0;
                menu_screen = 0; 
                return;
            case 6:
                select_option = 0;
                menu_screen = 0;
                draw_controlfan_options();
                return;

            case 7:
                select_option = 0;
                menu_screen = 0; 
                return;

            default:
               break;
        }
    
    }

    if(new_pad & BUTTON_TRIANGLE) {
        menu_screen = 0; return;
    }
   

    if(new_pad & BUTTON_UP) {

        frame_count = 32;

        ROT_DEC(select_option, 0, 7);
        
    }

    if(new_pad & BUTTON_DOWN) {

        frame_count = 32;
        
        ROT_INC(select_option, 7, 0);
        
    }
    
}

void draw_cache_external()
{
    int menu = menu_screen;

    LoadCacheDatas();
  
    if(ncache_list > 0) {
        menu_screen = 4;
        select_option = 0;
    } else return;

    while(menu_screen != 0) {
        flash = (frame_count >> 5) & 1;

        frame_count++;
        cls();

        update_twat();
        menu_screen = 5;
        draw_cachesel(28, 0);
    }

    menu_screen = menu;
}

void draw_cachesel(float x, float y)
{

    int n;

    float y2, x2;
    
    SetCurrentFont(FONT_TTF);

    // header title

    DrawBox(x, y, 0, 200 * 4 - 8, 20, 0x00000028);

    SetFontColor(0xffffffff, 0x00000000);

    SetFontSize(18, 20);

    SetFontAutoCenter(0);
  
    DrawFormatString(x, y, " %s", language[DRAWCACHE_CACHE]);
    
    x2= DrawFormatString(2000, 0, "hdd0: %.2fGB ", freeSpace[0]);
    x2 = 848 -(x2 - 2000) - x;
    DrawFormatString(x2, 0, "hdd0: %.2fGB ", freeSpace[0]);


    y += 24;

    DrawBox(x, y, 0, 200 * 4 - 8, 150 * 3 - 8, 0x00000028);

    
    x2 = x;
    y2 = y + 32;

    for(n = (select_option / 8) * 8; (n < (select_option / 8) * 8 + 8); n++) {
        if(n < ncache_list) {
            sprintf(temp_buffer, "%s (%1.2f GB)", cache_list[n].title_id, ((double) cache_list[n].size)/(1024.0*1024.*1024.0));
            DrawButton1_UTF8((848 - 520) / 2, y2, 520, temp_buffer, (flash && select_option == n));
        } else DrawButton1_UTF8((848 - 520) / 2, y2, 520, "", -1);
    
        y2+= 48;
    }

    SetCurrentFont(FONT_TTF);

    // draw game name

    DrawBox(x, y + 3 * 150, 0, 200 * 4 - 8, 40, 0x00000028);

    
    if(flash && cache_need_free != 0) {
        SetFontSize(20, 20);
        SetFontColor(0xffff00ff, 0x00000000);
        SetFontAutoCenter(1);
        DrawFormatString(0, y + 3 * 150 + 6, language[DRAWCACHE_ERRNEEDIT], cache_need_free);
        SetFontAutoCenter(0);

    } else if(select_option < ncache_list){
    

        SetFontColor(0xffffffff, 0x00000000);

        utf8_truncate(cache_list[select_option].title, temp_buffer, 65);

        temp_buffer[65] = 0;

        if(strlen(temp_buffer) < 50) SetFontSize(18, 32); 
        else SetFontSize(14, 32);

        SetFontAutoCenter(1);
  
        DrawFormatString(0, y + 3 * 150, temp_buffer);

        SetFontAutoCenter(0);
    
    
    }
    SetFontColor(0xffffffff, 0x00000000);


    tiny3d_Flip();

    ps3pad_read();

    if(new_pad & (BUTTON_CROSS | BUTTON_CIRCLE)) {
    
        if(select_option >= ncache_list) return;

        sprintf(temp_buffer, language[DRAWCACHE_ASKTODEL], cache_list[select_option].title_id);

        if(DrawDialogYesNo(temp_buffer) == 1) {
           
            sprintf(temp_buffer, "%s/cache/%s", self_path, cache_list[select_option].title_id);
            DeleteDirectory(temp_buffer);
            rmdir_secure(temp_buffer);
            LoadCacheDatas();

            if(ncache_list >= select_option) select_option = ncache_list - 1;

            if(ncache_list <= 0) {
                select_option = 0;
                menu_screen = 0; 
            }
        }
        new_pad = 0;
        return;
    }

    if(new_pad & BUTTON_TRIANGLE) {
        select_option = 0;
        menu_screen = 0; return;
    }
   

    if(new_pad & BUTTON_UP) {

        frame_count = 32;

        ROT_DEC(select_option, 0, ncache_list - 1)
        
    }

    if(new_pad & BUTTON_DOWN) {
        
        frame_count = 32;

        ROT_INC(select_option, ncache_list - 1, 0); 
        
    }
    
}

/******************************************************************************************************************************************************/
/* BDVDEMU FUNCTIONS                                                                                                                                  */
/******************************************************************************************************************************************************/

void unpatch_bdvdemu()
{
//LV2 Mount for 355 in his payload code

    int flag = 0;
    flag = lv2_unpatch_bdvdemu();
}


int patch_bdvdemu(u32 flags)
{
    int n;
    int usb = -1;

    static int one = 0;

    if(one) return 0; // only one time


    if(((flags & 1) && game_cfg.bdemu == 2) 
        || (!(flags & 1) && game_cfg.bdemu_ext == 2) || (flags & 2048) ) 
    {
        if(noBDVD == 2) flags= (1<<15); else return 0; // new to add BD-Emu 2
    }

    one = 1;

    if(noBDVD && (flags & (1<<15))) flags = 1;

    flags&= GAMELIST_FILTER;

    for(n = 1; n < 11; n++) {
        if(flags == (1 << n)) {usb = n - 1; break;}
    }

    if(usb == -1) bdvd_is_usb = 0; else bdvd_is_usb = 1;
/*
    if(usb < 0) {
        DrawDialogOK(language[PATCHBEMU_ERRNOUSB]);
        return -1;
    }
    */

    int flag = 0;
    flag = lv2_patch_bdvdemu(flags);

    return 0;
}

int move_origin_to_bdemubackup(char *path)
{

    
    if(strncmp(path, "/dev_usb00", 10) && strncmp(path, "/dev_hdd0", 9)) return 1;
    
    sprintf(temp_buffer, "%s/PS3_GAME/PS3PATH.BUP", path);
    sprintf(temp_buffer + 1024, "%s/PS3_GAME", path);

    if(SaveFile(temp_buffer, temp_buffer + 1024, strlen(temp_buffer + 1024))!=0) {
        
        sprintf(temp_buffer + 1024, language[MOVEOBEMU_ERRSAVE], temp_buffer);
        DrawDialogOK(temp_buffer + 1024);
        
        return -1;
    }
    
    if(!strncmp(temp_buffer, "/dev_hdd0", 9)) strncpy(temp_buffer + 9, "/PS3_GAME", 16);
        else
            strncpy(temp_buffer + 11, "/PS3_GAME", 16);

    // sys8_perm_mode(1);
    int n= sysLv2FsRename(temp_buffer  + 1024, temp_buffer);
    // sys8_perm_mode(0);

    if(n != 0)  {
        
        sprintf(temp_buffer + 256, language[MOVEOBEMU_ERRMOVE], temp_buffer);
        DrawDialogOK(temp_buffer + 256);

        return -1;
     }
    
    if(autolaunch < LAUCHMODE_STARTED) {
        sprintf(temp_buffer + 256, language[MOVEOBEMU_MOUNTOK], temp_buffer);
        DrawDialogOKTimer(temp_buffer + 256, 2000.0f);
    }

    return 0;
}

int move_bdemubackup_to_origin(u32 flags)
{
    int n;
    int usb = -1;

    static u32 olderrflags = 0;

    flags&= GAMELIST_FILTER;
  
    for(n = 0; n < 11; n++) {
        if(flags == (1 << n)) {usb = n - 1; break;}
    }

    if(usb < 0) {
        sprintf(temp_buffer, "/dev_hdd0/PS3_GAME");

        sprintf(temp_buffer + 256, "/dev_hdd0/PS3_GAME/PS3PATH.BUP");
     
    } else {

        sprintf(temp_buffer, "/dev_usb00%c/PS3_GAME", 48 + usb);

        sprintf(temp_buffer + 256, "/dev_usb00%c/PS3_GAME/PS3PATH.BUP", 48 + usb);
    }

    int file_size;
    char *file;

    sysFSStat dstat;
    if(sysLv2FsStat(temp_buffer, &dstat) != 0) return -1;

    file = LoadFile(temp_buffer + 256, &file_size);
    
    if(!file) return -1;

    memset(temp_buffer + 1024, 0, 0x420);
    
    if(file_size > 0x400) file_size = 0x400;

    memcpy(temp_buffer + 1024, file, file_size);

    free(file);

    for(n=0; n< 0x400; n++) {
        if(temp_buffer[1024 + n] == 0) break;
        if(((u8)temp_buffer[1024 + n]) < 32) {temp_buffer[1024 + n] = 0; break;}
    }
    
    
    if(strncmp(temp_buffer, temp_buffer + 1024, 10))  return -1; // if not /dev_usb00x return

    if(!strncmp(temp_buffer, "/dev_hdd0", 9)) {
    } else
        memcpy(temp_buffer + 1024, temp_buffer, 11); 

    // sys8_perm_mode(1);
    n= sysLv2FsRename(temp_buffer, temp_buffer + 1024);
    // sys8_perm_mode(0);

    if(n != 0)  {
        if(!(olderrflags & flags)) {
            sprintf(temp_buffer, language[MOVETBEMU_ERRMOVE], temp_buffer + 1024);
            DrawDialogOK(temp_buffer);
            olderrflags |= flags;
        }
        return -1;
    }

    return 0;
}