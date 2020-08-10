#include <string.h>
#include "tools.h"
#include "../gfx/gfxutils.h"
#include "../../libs/fatfs/ff.h"
#include "../../gfx/gfx.h"
#include "../../hid/hid.h"
#include "../../soc/gpio.h"
#include "../../utils/util.h"
#include "../../utils/types.h"
#include "../../libs/fatfs/diskio.h"
#include "../../storage/sdmmc.h"
#include "../../utils/sprintf.h"
#include "../../soc/fuse.h"
#include "../emmc/emmc.h"
#include "../common/common.h"
#include "../fs/fsactions.h"
#include "../fs/fsutils.h"
#include "../../mem/heap.h"
#include "../utils/utils.h"
#include "../fs/nca.h"

extern bool sd_mount();
extern void sd_unmount();
extern sdmmc_storage_t sd_storage;

void displayinfo(){
    gfx_clearscreen();

    FATFS *fs;
    DWORD fre_clust, fre_sect, tot_sect;
    u32 capacity;
    u8 fuse_count = 0;
    pkg1_info pkg1 = returnpkg1info();
    int res;

    for (u32 i = 0; i < 32; i++){
        if ((fuse_read_odm(7) >> i) & 1)
            fuse_count++;
    }

    SWAPCOLOR(COLOR_ORANGE);

    gfx_printf("Fuse count: %d\nPKG1 id: '%s'\n", fuse_count, pkg1.id);
    if (pkg1.ver >= 0)
        gfx_printf("PKG1 version: %d\n", pkg1.ver);

    gfx_printf("\n");

    print_biskeys();

    RESETCOLOR;
    gfx_printf("\n-----\n\n");

    SWAPCOLOR(COLOR_BLUE);

    if (!sd_mount()){
        gfx_printf("SD mount failed!\nFailed to display SD info\n");
    }
    else {
        gfx_printf("Getting storage info: please wait...");

        res = f_getfree("sd:", &fre_clust, &fs);
        gfx_printf("\nResult getfree: %d\n\n", res);

        tot_sect = (fs->n_fatent - 2) * fs->csize;
        fre_sect = fre_clust * fs->csize;
        capacity = sd_storage.csd.capacity;

        gfx_printf("Entire sd:\nSectors: %d\nSpace total: %d MB\n\n", capacity, capacity / 2048);
        gfx_printf("First partition on SD:\nSectors: %d\nSpace total: %d MB\nSpace free: %d MB\n\n", tot_sect, tot_sect / 2048, fre_sect / 2048);
    }

    RESETCOLOR;
    gfx_printf("Press any key to continue");
    hidWait();
}

void displaygpio(){
    gfx_clearscreen();
    gfx_printf("Updates gpio pins every 50ms:\nPress power to exit");
    msleep(200);
    while (1){
        msleep(10);
        gfx_con_setpos(0, 63);

        for (int i = 0; i <= 30; i++){
            gfx_printf("\nPort %d: ", i);
            for (int i2 = 7; i2 >= 0; i2--)
                gfx_printf("%d", gpio_read(i, (1 << i2)));
        }

        if (hidRead()->pow)
            break;
    }
}


// bad code ahead aaaa
int dumpfirmware(int mmc, bool daybreak){
    DIR dir;
    FILINFO fno;
    bool fail = false;
    int ret, amount = 0;
    char sysbase[] = "emmc:/Contents/registered";
    char *syspathtemp, *syspath, *sdpath, *sdbase;
    pkg1_info pkg1 = returnpkg1info();
    u32 timer = get_tmr_s();

    gfx_clearscreen();
    connect_mmc(mmc);
    mount_mmc("SYSTEM", 2);

    if (daybreak){
        if (!fsutil_checkfile("sd:/switch/prod.keys")){
            SWAPCOLOR(COLOR_RED);
            gfx_printf("prod.keys not found.\nPress any button to exit");
            hidWait();
            return true;
        }

        if (SetHeaderKey()){
            SWAPCOLOR(COLOR_RED);
            gfx_printf("Failed to find header key.\nPress any button to exit");
            hidWait();
            return true;
        }
    }

    gfx_printf("PKG1 version: %d\n", pkg1.ver);

    gfx_printf("Creating folders...\n");
    f_mkdir("sd:/tegraexplorer");
    f_mkdir("sd:/tegraexplorer/Firmware");

    sdbase = calloc(32 + strlen(pkg1.id), sizeof(char));
    if (daybreak) {
      sprintf(sdbase, "sd:/tegraexplorer/Firmware/%d (%s) - Daybreak", pkg1.ver, pkg1.id);
    }
    else {
      sprintf(sdbase, "sd:/tegraexplorer/Firmware/%d (%s) - ChoiNX", pkg1.ver, pkg1.id);
    }

    if (fsutil_checkfile(sdbase)){
        SWAPCOLOR(COLOR_RED);
        gfx_printf("Destination folder already exists.\nPress X to delete this folder, any other button to cancel\nPath: %s", sdbase);

        Inputs *input = hidWait();
        if (!input->x){
            free(sdbase);
            return true;
        }
        else {
            SWAPCOLOR(COLOR_WHITE);
            gfx_printf("\nDeleting folder..\n");
            fsact_del_recursive(sdbase);
        }
    }

    gfx_printf("\rOut: %s\n", sdbase);
    f_mkdir(sdbase);

    if ((ret = f_opendir(&dir, sysbase)))
        fail = true;

    gfx_printf("Starting dump...\n");
    SWAPCOLOR(COLOR_GREEN);

    printerrors = 0;

    while(!f_readdir(&dir, &fno) && fno.fname[0] && !fail){
        utils_copystring(fsutil_getnextloc(sysbase, fno.fname), &syspathtemp);

        if (fno.fattrib & AM_DIR){
            utils_copystring(fsutil_getnextloc(syspathtemp, "00"), &syspath);
        }
        else
            syspath = syspathtemp;

        if (daybreak){
            u8 ContentType = GetNcaType(syspath);

            if (ContentType < 0){
                fail = true;
                continue;
            }


            char *temp;
            utils_copystring(fsutil_getnextloc(sdbase, fno.fname), &temp);

            if (ContentType == 0x01){
                sdpath = calloc(strlen(temp) + 6, 1);
                strcpy(sdpath, temp);
                memcpy(sdpath + strlen(temp) - 4, ".cnmt.nca", 10);
                free(temp);
            }
            else {
                sdpath = temp;
            }
        }
        else
            utils_copystring(fsutil_getnextloc(sdbase, fno.fname), &sdpath);


        ret = fsact_copy(syspath, sdpath, 0);

        gfx_printf("%d %s\r", ++amount, fno.fname);

        if (ret != 0)
            fail = true;

        free(sdpath);
        free(syspathtemp);
        if (syspath != syspathtemp)
            free(syspath);
    }

    printerrors = 1;

    if (fail)
        gfx_printf("%k\n\nDump failed! Aborting (%d)", COLOR_RED, ret);

    gfx_printf("%k\n\nPress any button to continue...\nTime taken: %ds", COLOR_WHITE, get_tmr_s() - timer);
    free(sdbase);
    hidWait();

    return fail;
}
/*
void dumpusersaves(int mmc){
    connect_mmc(mmc);
    mount_mmc("USER", 2);
    gfx_clearscreen();

    gfx_printf("Creating folders...\n");
    f_mkdir("sd:/tegraexplorer");

    gfx_printf("Starting dump...\n");

    SWAPCOLOR(COLOR_GREEN);

    if(fsact_copy_recursive("emmc:/save", "sd:/tegraexplorer"))
        return;

    RESETCOLOR;
    gfx_printf("\n\nSaves are located in SD:/tegraexplorer/save\n");
    gfx_printf("Press any key to continue");
    hidWait();
}
*/

int format(int mode){
    gfx_clearscreen();
    int res;
    bool fatalerror = false;
    DWORD plist[] = {666, 61145088, 0, 0};
    u32 timer, totalsectors, alignedsectors, extrasectors;
    u8 *work;
    DWORD clustsize = 32768;
    BYTE formatoptions = 0;
    formatoptions |= (FM_FAT32);
    //formatoptions |= (FM_SFD);

    disconnect_mmc();

    timer = get_tmr_s();
    totalsectors = sd_storage.csd.capacity;

    gfx_printf("Initializing...\n");

    work = calloc(BUFSIZE, sizeof(BYTE));

    if (work == NULL){
        gfx_errDisplay("format", ERR_MEM_ALLOC_FAILED, 0);
        return 0;
    }

    if (mode == FORMAT_EMUMMC){
        if (totalsectors < 83886080){
            gfx_printf("%kYou seem to be running this on a <=32GB SD\nNot enough free space for emummc!", COLOR_RED);
            fatalerror = true;
        }
        else {
            totalsectors -= plist[1];
            alignedsectors = (totalsectors / 2048) * 2048;
            extrasectors = totalsectors - alignedsectors;
            plist[0] = alignedsectors;
            plist[1] += extrasectors;
            gfx_printf("\nStarting SD partitioning:\nTotalSectors:        %d\nPartition1 (SD):     %d\nPartition2 (EMUMMC): %d\n", plist[0] + plist[1], plist[0], plist[1]);
        }
    }
    else {
        plist[0] = totalsectors;
        plist[1] = 0;
    }

    if (!fatalerror){
        gfx_printf("\nPartitioning SD...\n");
        res = f_fdisk(0, plist, work);

        if (res){
            gfx_printf("%kf_fdisk returned %d!\n", COLOR_RED, res);
            fatalerror = true;
        }
        else
            gfx_printf("Done!\n");
    }

    if (!fatalerror){
        gfx_printf("\n\nFormatting Partition1...\n");
        res = f_mkfs("0:", formatoptions, clustsize, work, BUFSIZE * sizeof(BYTE));

        if (res){
            gfx_printf("%kf_mkfs returned %d!\n", COLOR_RED, res);
            fatalerror = true;
        }
        else
            gfx_printf("Smells like a formatted SD\n\n");
    }

    free(work);

    sd_unmount();

    if (!fatalerror){
        if (!sd_mount())
            gfx_printf("%kSd failed to mount!\n", COLOR_ORANGE);
        else {
            gfx_printf("Sd mounted!\n");
        }
    }

    connect_mmc(SYSMMC);

    gfx_printf("\nPress any button to return%k\nTotal time taken: %ds", COLOR_WHITE, (get_tmr_s() - timer));
    hidWait();
    return fatalerror;
}
