/**
 * @file tui_image.c
 * @author MiiKaa3
 * @brief Handles image rendering for sigma360. Image data is stored in the
 * pane's userptr attribute.
 */
#include "tui_image.h"
#include <stdio.h>
#include <string.h>

#include "tui.h"
#include "const.h"
#include "tui_image.h"

/**
 * Clears the current image attached to the pane. Destroys the backing sub-pane
 * supporting the image, and empties the imageData struct, without deleting the
 * struct.
 * @param preview Pane with image attached to be cleared.
 */
void preview_image_clear(Pane* preview)
{
    imageData* data = ncplane_userptr(preview->content);
    if (preview->content) {
        ncplane_destroy(preview->content);
    }
    free(data->image);
    ncvisual_destroy(data->ncimage);
}

/**
 * Generates the image to be displayed and draws it to screen. Places necessary
 * image information in the pane's imageData structure attached to the userptr.
 * imageData struct is heap-allocated; imageData.image is heap-allocated;
 * imageData.ncimage needs to be destroyed.
 * @param preview Pane to draw the desired image to.
 * @param path    Absolute path of image.
 * @returns
 *      BAD         given failure to generate image backing pane.
 *      BAD_SIZE    given window size is too small.
 *      BAD_IMAGE   given no image exists at path or failure to blit.
 *      GOOD        upon success.
 */
int preview_image_show(Pane* preview, const char* path)
{
    unsigned rows;
    unsigned cols;
    ncplane_dim_yx(plane, &rows, &cols);
    if (rows < MIN_ROWS || cols < MIN_COLS) { /* too small to hold anything */
        sigma360_tui_image_clear();
        return BAD_SIZE;
    }
    // Inset so image is within pane borders.
    rows -= 2;
    cols -=2; 

    imageData* data = ncplane_userptr(preview->content);
    // Initialise the imageData tag along struct
    if (!data) {
        data = malloc(sizeof(imageData));
        data->image = strdup(defaultImage);
        ncplane_set_userptr(preview->content, data);
    }
    // Catch if image is already displayed in pane
    if (strcmp(path, data->image) == 0) {
        return GOOD;
    }
    // Else we want to display the image
    preview_image_clear(preview);

    struct ncvisual* ncimage = ncvisual_from_file(path);
    if (!ncimage) {
        // Failed to fetch visual data from path. This hits if there is no image
        // at the current directory, i.e. get_thumbnails is still working or
        // course is not current.
        return BAD_IMAGE;
    }

    struct ncplane_options imagePaneOpts = {
        .y = 1, 
        .x = 1, /* relative to plane's top-left */
        .rows = rows, 
        .cols = cols,
    };
    struct ncplane* imageDisplay = ncplane_create(preview->content, 
            &imagePaneOpts);
    if (!imageDisplay) {
        ncvisual_destroy(ncimage);
        return BAD;
    }

    struct ncvisual_options visualOpts = {
        .n = imageDisplay,
        .scaling = NCSCALE_SCALE,
        .blitter = NCBLIT_PIXEL,
        // Centers the image in the pane
        .flags = NCVISUAL_OPTION_HORALIGNED | NCVISUAL_OPTION_VERALIGNED,
        .x = NCALIGN_CENTER,
        .y = NCALIGN_CENTER
    };
    if (!ncvisual_blit(ncplane_notcurses(plane->content), 
                ncimage, &visualOpts)) {
        ncvisual_destroy(ncimage);
        ncplane_destroy(imageDisplay);
        return BAD_IMAGE;
    }
    data->ncimage = ncimage;
    data->image = strdup(path);
    return GOOD;
}

/**
 * Being a good memory citizen and cleanuping up the imageData structure.
 * All items in the struct, including the struct itself, are heap-allocated and
 * needs to be freed appropriately.
 * @param preview Pane with attached imageData struct to clean up.
 */
void cleanup_image(Pane* preview)
{
    imageData* data = ncplane_get_userptr(preview->content);
    if (data) {
        preview_clear_image(preview);
        free(data);
    }
}
