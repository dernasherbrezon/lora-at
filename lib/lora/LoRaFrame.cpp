#include "LoraFrame.h"
#include <stdlib.h>

void LoRaFrame_destroy(LoRaFrame *frame) {
    if( frame == NULL ) {
        return;
    }
    if( frame->data != NULL ) {
        free(frame->data);
    }
    free(frame);
}