//
// Created by root on 18-9-14.
//

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
            }
        }
    }
    lck.unlock();
    total_pending = wait_token + out_standing + net_send + wait_disk + net_rcv
                    + complete;
    obj.merge(json_obj("retry", retry));
    obj.merge(json_obj("wait_token", wait_token));
    obj.merge(json_obj("out_standing", out_standing));
    obj.merge(json_obj("net_send", net_send));
    obj.merge(json_obj("wait_disk", wait_disk));
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
    if(t < 1000) return to_string(t)+string("us");
    t/=1000;
    if(t<1000) return to_string(t)+string("ms");
    t/=1000;
    if(t<60) return to_string(t)+string("s");
    t/=60;
    return to_string(t)+string("min");
}

json_obj cli_ser::history()
{
    unique_lock<mutex> lck(m);
    string             t_usec = time_unit_transfer((long)ave_task_usec);
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
