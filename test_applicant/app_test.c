//
// Created by root on 18-9-3.
//

#include <CUnit/CUnit.h>
#include <test_frame.h>
#include "node_inapp.h"
#include "wait_token_list.h"
#include "token_grp.h"
#include "token_manager.h"

int main(int argc, char *argv)
{
    test_frame_t frame;
    
    test_frame_init(&frame);
    
    node_inapp_suit_init(&frame);
    
    wait_list_suit_init(&frame);
    
    token_reqgrp_suit_init(&frame);
    
    token_manager_suit_init(&frame);
    
    base_node_suit_init(&frame);
   
    frame.run(&frame);
    
    test_frame_exit(&frame);
    return 0;
}