create table if not exists eventLog
(
    event_type      tinyint unsigned,
    value_int1      int,
    value_int2      int,
    value_int3      int,
    value_float1    float,
    value_float2    float,
    value_float3    float,
    mod_timestamp   timestamp
);

create index idx_eventLog_time
on eventLog(mod_timestamp);

create index idxbm_eventLog_event_type
on eventLog(event_type);
