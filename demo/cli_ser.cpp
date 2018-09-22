//
// Created by root on 18-9-14.
//

#include <sysqos_dump_interface.h>
#include "cli_ser.h"
#include "task.h"
#include "client.h"
#include "server.h"

json_obj cli_ser::to_json_obj()
{
    json_obj obj("client", cli->name());
    obj.merge(json_obj("server", srv->name()));
    obj.merge(analysis_tasks());
    obj.merge(history());
    obj.merge(quotas());
    obj.merge(
            json_obj("disk_usage", to_string(srv->last_percent) + string("%")));
    obj.merge(json_obj("net_usage",
                       to_string(srv->net_last_percent) + string("%")));
    return obj;
}

cli_ser::cli_ser(client *_cli, server *_srv) :
        cli(_cli), srv(_srv), task_num(0), ave_task_usec(0), fail_num(0)
{
}

void cli_ser::add_task(task *t)
{
    unique_lock<mutex> lck(m);
    tasks.insert(make_pair(t->task_id, t));
}

void cli_ser::rm_task(task *t)
{
    unique_lock<mutex> lck(m);
    auto               it = tasks.find(t->task_id);
    tasks.erase(it);
}

json_obj cli_ser::analysis_tasks()
{
    json_obj obj;
    obj.set_dic();
    int                retry         = 0;
    int                wait_token    = 0;
    int                wait_grp      = 0;
    int                out_standing  = 0;
    int                net_send      = 0;
    int                wait_disk     = 0;
    int                net_rcv       = 0;
    int                complete      = 0;
    int                total_pending = 0;
    int                last_rcvid    = 0;
    int                last_failid   = 0;
    unique_lock<mutex> lck(m);
    for ( auto         it : tasks )
    {
        task *t = it.second;
        retry += t->retry_num;
        if ( t->retry_num )
        {
            last_failid = t->task_id;
        }
        {
            switch ( t->state )
            {
                case task_stat_out_standing:
                    ++out_standing;
                    break;
                case task_stat_wait_token:
                    ++wait_token;
                    break;
                case task_stat_net_send:
                    ++net_send;
                    break;
                case task_stat_wait_disk:
                    ++wait_disk;
                    break;
                case task_stat_net_rcv:
                    last_rcvid = t->task_id;
                    ++net_rcv;
                    break;
                case task_stat_complete:
                    ++complete;
                    break;
                case task_stat_wait_grp:
                    ++wait_grp;
                    break;
            }
        }
    }
    lck.unlock();
    total_pending =
            wait_token + out_standing + wait_grp + net_send + wait_disk +
            net_rcv
            + complete;
    obj.merge(json_obj("retry", retry));
    obj.merge(json_obj("wait_token", wait_token));
    obj.merge(json_obj("out_standing", out_standing));
    obj.merge(json_obj("net_send", net_send));
    obj.merge(json_obj("wait_disk", wait_disk));
    obj.merge(json_obj("wait_grp", wait_grp));
    obj.merge(json_obj("net_rcv", net_rcv));
    obj.merge(json_obj("complete", complete));
    obj.merge(json_obj("total_pending", total_pending));
    obj.merge(json_obj("last_rcvid", last_rcvid));
    obj.merge(json_obj("last_failid", last_failid));
    obj.merge(json_obj("fail_num", fail_num));
    return obj;
}

string time_unit_transfer(long t)
{
    if ( t < 1000 )
    { return to_string(t) + string("us"); }
    t /= 1000;
    if ( t < 1000 )
    { return to_string(t) + string("ms"); }
    t /= 1000;
    if ( t < 60 )
    { return to_string(t) + string("s"); }
    t /= 60;
    return to_string(t) + string("min");
}

json_obj cli_ser::history()
{
    unique_lock<mutex> lck(m);
    string             t_usec = time_unit_transfer((long) ave_task_usec);
    long               t_n    = task_num;
    lck.unlock();
    json_obj obj("t_usec", t_usec);
    obj.merge(json_obj("t_n", (long long) t_n));
    return obj;
}

void cli_ser::update(task *t)
{
    unique_lock<mutex> lck(m);
    long               new_usecs = t->usecs();
    ave_task_usec =
            (ave_task_usec * task_num + new_usecs) / (task_num + 1);
    ++task_num;
    if ( t->retry_num > 0 )
    {
        ++fail_num;
    }
    
}

json_obj cli_ser::quotas()
{
    sysqos_disp_dump_info_t disp_info;
    sysqos_app_dump_info_t  app_info;
    sysqos_dump_app(&cli->ops, (void *) srv->name().c_str(), &app_info);
    sysqos_dump_disp(&srv->ops, (void *) cli->name().c_str(), &disp_info);
    json_obj obj("token_inuse", (long long) app_info.token_inuse);
    obj.merge(json_obj("app_version", (long long) app_info.version));
    obj.merge(json_obj("disp_version", (long long) disp_info.version));
    obj.merge(json_obj("app_quota", (long long) app_info.token_quota));
    obj.merge(json_obj("app_quotanew", (long long) app_info.token_quota_new));
    obj.merge(json_obj("app_target", (long long) app_info.token_quota_target));
    obj.merge(json_obj("respond_step", (long long) app_info.respond_step));
    obj.merge(json_obj("disp_quota", (long long) disp_info.token_quota));
    obj.merge(json_obj("press", (long long) disp_info.press));
    obj.merge(json_obj("disp_quotanew", (long long) disp_info.token_quota_new));
    obj.merge(json_obj("min", (long long) disp_info.min));
    obj.merge(json_obj("dynamic", (long long) disp_info.token_dynamic));
    obj.merge(json_obj("static", (long long) disp_info.token_used_static));
    obj.merge(json_obj("percent",
                       to_string((disp_info.token_quota) * 100 /
                                 (disp_info.token_used_static +
                                  disp_info.token_dynamic)) + string("%")));
    obj.merge(json_obj("dynamic_percent",
                       to_string((disp_info.token_quota -
                                  disp_info.min) * 100 /
                                 disp_info.token_dynamic) + string("%")));
    return obj;
}
