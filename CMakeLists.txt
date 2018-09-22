cmake_minimum_required(VERSION 3.7)
project(datastor_qos)

set(CMAKE_CXX_STANDARD 11)

add_definitions(-std=c++11)

set(CunitSource
        /usr/local/lib/libcunit.la)

set(SOURCE_FILES
        list_head.h
        sysqos_interface.h  sysqos_interface.c
        sysqos_token_reqgrp_manager.c
        sysqos_token_reqgrp_manager.h
        hash_table.c hash_table.h
        memory_cache.c memory_cache.h
        sysqos_alloc.c sysqos_alloc.h sysqos_common.h
        safe_rw_list.c safe_rw_list.h
        sysqos_container_item.h  sysqos_nodereq_list.c sysqos_nodereq_list.h
        sysqos_policy_param.h sysqos_dispatch_base_node.c sysqos_dispatch_base_node.h
        sysqos_protocol.h sysqos_token_reqgrp.c sysqos_token_reqgrp.h
        sysqos_dispatch_node.c sysqos_dispatch_node.h
        sysqos_dispatch_manager.c sysqos_dispatch_manager.h sysqos_token_global.c
        sysqos_token_global.h sysqos_app_node.c sysqos_app_node.h
        sysqos_token_update.c sysqos_token_update.h sysqos_update_node.c
        sysqos_update_node.h
        sysqos_waitincrease_list.c sysqos_waitincrease_list.h count_controller.c
        count_controller.h fence_executor.c fence_executor.h sysqos_type.h
        sysqos_dump_interface.c sysqos_dump_interface.h)

include_directories("test_lib")
include_directories(".")
include_directories("/usr/local/include")
include_directories("/usr/local/include/lt_data")

add_library(libtest test_lib/sysqos_test_lib.c test_lib/sysqos_test_lib.h test_lib/test_frame.c test_lib/test_frame.h sysqos_type.h)
target_link_libraries(libtest pthread)

add_library(libqos ${SOURCE_FILES} sysqos_type.h sysqos_msg.h sysqos_resource.h)
target_link_libraries(libqos pthread libtest)

add_compile_options("-fprofile-arcs")
add_compile_options("-ftest-coverage")

add_executable(test_app2disp ${SOURCE_FILES} ${CunitSource}
        test_app2dispatch/app2disp_test.c
        test_app2dispatch/test_memory_cache.c test_app2dispatch/test_memory_cache.h
        test_app2dispatch/test_rw_list.c test_app2dispatch/test_rw_list.h
        test_app2dispatch/test_hashtab.c test_app2dispatch/test_hashtab.h ${CUnit}

        test_applicant/base_node.c test_applicant/base_node.h
        test_applicant/node_inapp.c test_applicant/node_inapp.h
        test_applicant/wait_token_list.c test_applicant/wait_token_list.h
        test_applicant/token_grp.c test_applicant/token_grp.h
        test_applicant/token_manager.c test_applicant/token_manager.h

        test_dispatch/node_indisp.c test_dispatch/node_indisp.h
        test_dispatch/wait_increase.c test_dispatch/wait_increase.h
        test_dispatch/token_global.c test_dispatch/token_global.h test_dispatch/dispatch_manager.c test_dispatch/dispatch_manager.h)
target_link_libraries(test_app2disp pthread  libtest gcov)


add_executable(demo ${SOURCE_FILES} ${CunitSource} demo/sysqos_demo_cmds.h demo/sysqos_demo_cmds.cpp
        demo/demo.cpp demo/demo.h test_lib/sysqos_test_lib.c test_lib/sysqos_test_lib.h demo/client.cpp demo/client.h demo/server.cpp demo/server.h demo/client_group.cpp demo/client_group.h demo/server_group.cpp demo/server_group.h demo/demo_obj.cpp demo/demo_obj.h demo/myevent_hander.cpp demo/myevent_hander.h demo/task.cpp demo/task.h demo/task_group.cpp demo/task_group.h demo/cli_ser.cpp demo/cli_ser.h demo/circle_increse_safe.cpp demo/circle_increse_safe.h demo/utime.cpp demo/utime.h demo/Timer.cpp demo/Timer.h sysqos_msg.h)
target_link_libraries(demo pthread  libtest  gcov boost_system boost_thread datastor_utils)