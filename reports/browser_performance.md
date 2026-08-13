# Browser 性能（V14）

已实现：stretch-only redraw（STF 改变不重新采样）、tile LRU（64）、
robust Auto Global（pan/zoom 无闪烁）、--reset-stf。
完整 profile（pan/zoom/STF 响应、10 分钟内存有界验证）延后执行。
