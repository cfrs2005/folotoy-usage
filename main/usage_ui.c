// Desktop usage instrument: parallel quota columns, direct button lenses, no menus.
#include "usage_ui.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
LV_FONT_DECLARE(usage_font);
LV_FONT_DECLARE(usage_font_small);
LV_FONT_DECLARE(usage_font_medium);
LV_FONT_DECLARE(usage_font_title);
LV_FONT_DECLARE(usage_font_number);
LV_FONT_DECLARE(usage_font_clock);
LV_FONT_DECLARE(usage_font_token);
#define PAPER 0xF4F3EA
#define INK 0x202B2D
#define MUTED 0x536264
#define RED 0xB64231
#define BLUE 0x225A81
#define TRACK 0xDCDDD3
static lv_obj_t *screen,*name_label,*clock_label,*date_label,*page_label,*avatar,*initial,*page_marks[2];
static uint8_t avatar_pixels[3200];
static lv_image_dsc_t avatar_image;
static int visible_page=-1;
typedef struct { lv_obj_t *object; uint32_t color; double value; } meter_t;
typedef struct {
    lv_obj_t *state,*labels[2],*values[2],*bottom[2],*cost_label[2];
    meter_t meters[2];
} provider_ui_t;
static provider_ui_t cards[2];
static lv_obj_t *box(lv_obj_t *parent,int x,int y,int w,int h,uint32_t color)
{
    lv_obj_t *o=lv_obj_create(parent);lv_obj_remove_style_all(o);
    lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(o,lv_color_hex(color),0);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);return o;
}
static lv_obj_t *text(lv_obj_t *parent,int x,int y,int w,const char *value,const lv_font_t *font,uint32_t color)
{
    lv_obj_t *o=lv_label_create(parent);lv_obj_set_pos(o,x,y);lv_obj_set_width(o,w);
    lv_obj_set_style_text_font(o,font,0);lv_obj_set_style_text_color(o,lv_color_hex(color),0);
    lv_label_set_long_mode(o,LV_LABEL_LONG_CLIP);lv_label_set_text(o,value);return o;
}
static void put(lv_obj_t *o,const char *value)
{
    if(strcmp(lv_label_get_text(o),value)!=0)lv_label_set_text(o,value);
}
static void font(lv_obj_t *o,const lv_font_t *value)
{
    if(lv_obj_get_style_text_font(o,0)!=value)lv_obj_set_style_text_font(o,value,0);
}
static void show(lv_obj_t *o,bool visible)
{
    if(visible)lv_obj_remove_flag(o,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);
}
// Ten physical-looking meter divisions, drawn without forty extra UI objects.
static void draw_meter(lv_event_t *event)
{
    meter_t *m=lv_event_get_user_data(event);
    lv_area_t bounds;lv_obj_get_coords(m->object,&bounds);
    lv_layer_t *layer=lv_event_get_layer(event);
    lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);d.bg_opa=LV_OPA_COVER;
    int width=lv_area_get_width(&bounds);
    double percent=m->value<0?0:m->value>100?100:m->value;
    for(int i=0;i<10;i++) {
        lv_area_t segment={.x1=bounds.x1+i*width/10,.y1=bounds.y1,
                           .x2=bounds.x1+(i+1)*width/10-3,.y2=bounds.y2};
        d.bg_color=lv_color_hex(TRACK);lv_draw_rect(layer,&d,&segment);
        double portion=(percent-i*10)/10;
        if(portion>0) {
            if(portion>1)portion=1;
            int filled=(int)ceil(lv_area_get_width(&segment)*portion);
            segment.x2=segment.x1+filled-1;
            d.bg_color=lv_color_hex(m->color);lv_draw_rect(layer,&d,&segment);
        }
    }
}
static void amount(char *out,size_t size,double value,bool money)
{
    if(value<0)snprintf(out,size,"--");
    else if(money && value>=1e15)snprintf(out,size,"$%.1fP",value/1e15);
    else if(money && value>=1e12)snprintf(out,size,"$%.1fT",value/1e12);
    else if(money && value>=1e9)snprintf(out,size,"$%.1fB",value/1e9);
    else if(money && value>=1e6)snprintf(out,size,"$%.1fM",value/1e6);
    else if(money && value>=1000)snprintf(out,size,"$%.0f",value);
    else if(money)snprintf(out,size,"$%.2f",value);
    else if(value>=1e15)snprintf(out,size,"%.1fP",value/1e15);
    else if(value>=1e12)snprintf(out,size,"%.1fT",value/1e12);
    else if(value>=1e9)snprintf(out,size,"%.2fB",value/1e9);
    else if(value>=1e6)snprintf(out,size,"%.1fM",value/1e6);
    else if(value>=1e3)snprintf(out,size,"%.1fK",value/1e3);
    else snprintf(out,size,"%.0f",value);
}
void usage_ui_create(void)
{
    screen=box(NULL,0,0,240,320,PAPER);
    initial=text(screen,16,10,40,"U",&usage_font_number,INK);
    avatar=lv_image_create(screen);lv_obj_set_pos(avatar,10,10);show(avatar,false);
    name_label=text(screen,60,9,78,"UsageHub",&usage_font,INK);
    clock_label=text(screen,139,5,91,"--:--",&usage_font_clock,INK);
    lv_obj_set_style_text_align(clock_label,LV_TEXT_ALIGN_RIGHT,0);
    date_label=text(screen,60,34,112,"---- -- --",&usage_font_small,MUTED);
    page_label=text(screen,188,34,45,"额度",&usage_font_small,MUTED);
    page_marks[0]=box(screen,237,13,2,10,INK);
    page_marks[1]=box(screen,237,28,2,10,TRACK);
    for(int i=0;i<2;i++) {
        uint32_t color=i?BLUE:RED;
        lv_obj_t *panel=box(screen,8,65+i*128,224,119,PAPER);
        lv_obj_t *strip=box(panel,0,0,224,25,color);
        text(strip,9,1,112,i?"Codex":"Claude",&usage_font_title,0xFFFFFF);
        cards[i].state=text(strip,122,5,94,"等待数据",&usage_font_small,0xFFFFFF);
        lv_obj_set_style_text_align(cards[i].state,LV_TEXT_ALIGN_RIGHT,0);
        for(int j=0;j<2;j++) {
            int x=10+j*111;
            cards[i].labels[j]=text(panel,x,32,99,j?"7天 已用":"5小时 已用",&usage_font_small,MUTED);
            cards[i].values[j]=text(panel,x-2,45,103,"--%",&usage_font_number,INK);
            meter_t *meter=&cards[i].meters[j];meter->color=color;meter->value=-1;
            meter->object=box(panel,x,89,98,7,PAPER);
            lv_obj_add_event_cb(meter->object,draw_meter,LV_EVENT_DRAW_MAIN,meter);
            cards[i].cost_label[j]=text(panel,x,80,99,"API估算",&usage_font_small,MUTED);
            show(cards[i].cost_label[j],false);
            cards[i].bottom[j]=text(panel,x,101,101,"重置时间未知",&usage_font_small,MUTED);
        }
    }
    lv_screen_load(screen);
    lv_mem_monitor_t memory;lv_mem_monitor(&memory);
    ESP_LOGI("usage","UI pool: %u bytes free, largest %u",(unsigned)memory.free_size,(unsigned)memory.free_biggest_size);
}
void usage_ui_set_profile(const char *name,const uint8_t *rgb565,size_t bytes)
{
    if(name && name[0])put(name_label,name);
    if(rgb565 && (bytes==1568 || bytes==3200)) {
        memcpy(avatar_pixels,rgb565,bytes);
        unsigned size=bytes==3200?40:28;
        avatar_image.header.magic=LV_IMAGE_HEADER_MAGIC;avatar_image.header.cf=LV_COLOR_FORMAT_RGB565;
        avatar_image.header.w=size;avatar_image.header.h=size;avatar_image.header.stride=size*2;
        avatar_image.data_size=bytes;avatar_image.data=avatar_pixels;
        lv_image_set_src(avatar,&avatar_image);
        lv_image_set_scale(avatar,size==40?256:366);
        lv_obj_set_pos(avatar,size==40?10:16,size==40?10:16);
        show(avatar,true);show(initial,false);
    }
}
void usage_ui_update(const usage_snapshot_t *data,bool available,bool old,const char *status,int page,int battery,time_t now,bool remaining)
{
    (void)battery;
    char buf[80];
    if(now>1700000000) {
        struct tm t;localtime_r(&now,&t);
        strftime(buf,sizeof(buf),"%H:%M",&t);put(clock_label,buf);
        strftime(buf,sizeof(buf),"%Y-%m-%d",&t);put(date_label,buf);
    }
    if(visible_page!=page) {
        visible_page=page;
        for(int i=0;i<2;i++)lv_obj_set_style_bg_color(page_marks[i],lv_color_hex(i==page?INK:TRACK),0);
    }
    bool updating=strcmp(status,"连接中")==0 || strcmp(status,"更新中")==0;
    put(page_label,updating?"更新中":page?"用量":"额度");
    bool offline=strncmp(status,"离线",strlen("离线"))==0 || strncmp(status,"更新失败",strlen("更新失败"))==0;
    bool unpaired=strncmp(status,"授权失效",strlen("授权失效"))==0 || strstr(status,"配置授权")!=NULL;
    for(int i=0;i<2;i++) {
        const usage_provider_t *p=&data->providers[i];
        bool missing=!available || (page?!p->tokens_ok:!p->present);
        bool stale=old || (page?p->tokens_stale:p->stale);
        put(cards[i].state,unpaired?"需要配对":offline?"离线缓存":missing?"等待数据":stale?"数据较旧":"");
        for(int j=0;j<2;j++) {
            show(cards[i].meters[j].object,page==0);show(cards[i].cost_label[j],page!=0);
            if(page==0) {
                const usage_window_t *w=available&&p->present?usage_select_window(p,i==1,j?10080:300):NULL;
                double value=usage_display_percent(w?w->used:-1,remaining);
                snprintf(buf,sizeof(buf),"%s %s",j?"7天":"5小时",remaining?"剩余":"已用");put(cards[i].labels[j],buf);
                if(value<0)snprintf(buf,sizeof(buf),"--%%");else snprintf(buf,sizeof(buf),"%.0f%%",value);
                font(cards[i].values[j],value>100?&usage_font_token:&usage_font_number);put(cards[i].values[j],buf);
                meter_t *meter=&cards[i].meters[j];
                if(meter->value!=value) {meter->value=value;lv_obj_invalidate(meter->object);}
                usage_reset_text(buf,sizeof(buf),w?w->reset:-1,now);
                const char *reset=strncmp(buf,"还剩 ",strlen("还剩 "))==0?buf+strlen("还剩 "):buf;
                font(cards[i].bottom[j],&usage_font_small);lv_obj_set_y(cards[i].bottom[j],101);put(cards[i].bottom[j],reset);
            } else {
                put(cards[i].labels[j],j?"累计 Token":"今日 Token");
                amount(buf,sizeof(buf),available&&p->tokens_ok?(j?p->total_tokens:p->today_tokens):-1,false);
                font(cards[i].values[j],&usage_font_token);put(cards[i].values[j],buf);
                amount(buf,sizeof(buf),available&&p->tokens_ok?(j?p->total_cost:p->today_cost):-1,true);
                font(cards[i].bottom[j],&usage_font_medium);lv_obj_set_y(cards[i].bottom[j],95);put(cards[i].bottom[j],buf);
            }
        }
    }
}
