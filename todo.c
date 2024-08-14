#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <leif/leif.h>
#include <stdlib.h>
#include <string.h>

#define WIN_MARGIN 20.0f

typedef enum {
  TAB_DASHBOARD = 0,
  TAB_NEW_TASK
} gui_tab;


typedef enum {
  FILTER_ALL = 0,
  FILTER_IN_PROGRESS,
  FILTER_COMPLETED,
  FILTER_LOW,
  FILTER_MEDIUM,
  FILTER_HIGH
} todo_filter;


typedef enum {
  PRIORITY_LOW = 0,
  PRIORITY_MEDIUM,
  PRIORITY_HIGH,
  PRIORITY_COUNT
} entry_priority;


typedef struct {
  bool completed;
  char* desc, *date;

  entry_priority priority;
} task_entry;


static int winw = 1280, winh = 720;
static LfFont titlefont, textfont;
static todo_filter current_filter;
static gui_tab current_tab;

static task_entry* entries[1024];
static uint32_t numentries = 0;

static LfTexture removetexture, backtexture;

static LfInputField new_task_input;
static char new_task_input_buf[512];


char* get_command_output(const char* cmd) {
  FILE *fp;
  char buffer[1024];
  char* result = 0; 
  size_t result_size = 0;


  // Opening a new pipe with the fiven command 
  fp = popen(cmd, "r");
  if (fp == NULL) {
    printf("Failed to run command\n");
    return NULL;
  }

  // Reading the output 
  while (fgets(buffer, sizeof(buffer), fp) != NULL) {
    size_t buffer_len = strlen(buffer);
    char *temp = realloc(result, result_size + buffer_len + 1);
    if (temp == NULL) {
      printf("Memory allocation failed\n");
      free(result);
      pclose(fp);
      return NULL;
    }
    result = temp;
    strcpy(result + result_size, buffer);
    result_size += buffer_len;
  }
  pclose(fp);
  return result;
}


static void rendertopbar() {
  lf_push_font(&titlefont);
  lf_text("Your To Do");
  lf_pop_font();

  {
    const float width = 160.0f;

    lf_set_ptr_x_absolute(winw - width - WIN_MARGIN * 2.0f);
    LfUIElementProps props = lf_get_theme().button_props;
    props.margin_left = 0.0f;
    props.margin_right = 0.0f;
    props.color = (LfColor){134, 187, 216, 255};
    props.border_width = 0.0f;
    props.corner_radius = 4.0f;
    lf_push_style_props(props);
    lf_set_line_should_overflow(false);
    if(lf_button_fixed("New Task", width, -1) == LF_CLICKED) {
      current_tab = TAB_NEW_TASK;
    };
    lf_set_line_should_overflow(true);
    lf_pop_style_props();
  }
}


static void renderfilters() {
  
  const uint32_t numfilters = 6;
  static const char* filters[] = {
    "ALL", "IN PROGRESS", "COMPLETED", "LOW", "MEDIUM", "HIGH"
  };

  LfUIElementProps props = lf_get_theme().text_props;
  props.color = LF_NO_COLOR;
  props.margin_left = 10.0f;
  props.margin_right = 10.0f;
  props.margin_top = 30.0f;
  props.padding = 10.0f;
  props.border_width = 0.0f;
  props.color = LF_NO_COLOR;
  props.text_color = LF_WHITE;
  props.corner_radius = 8.0f;

  float width = 0.0f;
  float ptrx_before = lf_get_ptr_x();
  float ptry_before = lf_get_ptr_y();
  lf_push_style_props(props);
  lf_set_no_render(true);
  lf_set_ptr_y_absolute(lf_get_ptr_y() + 50.0f);
  for(uint32_t i = 0; i< numfilters; i++) {
    lf_button(filters[i]);
  }
  lf_set_no_render(false);
  lf_set_ptr_y_absolute(ptry_before);

  width = lf_get_ptr_x() - ptrx_before - props.margin_right - props.padding;


  lf_set_ptr_x_absolute(winw - width - WIN_MARGIN);

  lf_set_line_should_overflow(false);
  for(uint32_t i = 0; i < numfilters; i++) {
    props.color = (current_filter == i) ? (LfColor){255, 255, 255, 50} : LF_NO_COLOR;
    lf_push_style_props(props);
    if(lf_button(filters[i]) == LF_CLICKED) {
      current_filter = (todo_filter)i;
    }
    lf_pop_style_props();
  }
  lf_set_line_should_overflow(true);
    
}


static void renderentries() {
  
  lf_div_begin(((vec2s){lf_get_ptr_x(), lf_get_ptr_y()}),
                ((vec2s){winw - lf_get_ptr_x() - WIN_MARGIN, 
                (winh - lf_get_ptr_y() - WIN_MARGIN)}), 
                true);
  
  uint32_t renderedcount = 0;
  for(uint32_t i = 0; i < numentries; i++) {
    task_entry* entry = entries[i];
    if(current_filter == FILTER_LOW && entry->priority != PRIORITY_LOW) continue;
    if(current_filter == FILTER_MEDIUM && entry->priority != PRIORITY_MEDIUM) continue;
    if(current_filter == FILTER_HIGH && entry->priority != PRIORITY_HIGH) continue;
    if(current_filter == FILTER_COMPLETED && !entry->completed) continue;
    if(current_filter == FILTER_IN_PROGRESS && entry->completed) continue;

    float priority_size = 15.0f;
    float ptry_before = lf_get_ptr_y();

    lf_set_ptr_y_absolute(lf_get_ptr_y() + priority_size);
    lf_set_ptr_x_absolute(lf_get_ptr_x() + 5.0f);
    switch(entry->priority) {
      case PRIORITY_LOW: {
        lf_rect(priority_size, priority_size, (LfColor){76, 175, 80, 255}, 4.0f);
        break;
      }
      case PRIORITY_MEDIUM: {
        lf_rect(priority_size, priority_size, (LfColor){255, 235, 59, 255}, 4.0f);
        break;
      }
      case PRIORITY_HIGH: {
        lf_rect(priority_size, priority_size, (LfColor){244, 67, 54, 255}, 4.0f);
        break;
      }
    }
    lf_set_ptr_y_absolute(ptry_before);

    {
      LfUIElementProps props = lf_get_theme().button_props;
      props.color = LF_NO_COLOR;
      props.border_width = 0.0f; props.padding = 0.0f; props.margin_top = 0.0f; props.margin_left = 10.0f;
      lf_push_style_props(props);
      if(lf_image_button(((LfTexture){.id = removetexture.id, .width = 20, .height = 20})) == LF_CLICKED) {
        for(uint32_t j = i; j < numentries; j++) {
          entries[j] = entries[j + 1];
        }
        numentries--;
      }
      lf_pop_style_props(); 
    }
    {
      LfUIElementProps props = lf_get_theme().checkbox_props;
      props.border_width = 1.0f; props.corner_radius = 0.0f; props.margin_top = 0; props.padding = 5.0f;
      props.margin_left = 5.0f;
      props.color = lf_color_from_zto((vec4s){0.05f, 0.05f, 0.05f, 1.0f});
      lf_push_style_props(props);
      if(lf_checkbox("", &entry->completed, LF_NO_COLOR, ((LfColor){65, 167, 204, 255})) == LF_CLICKED) {
        
      }
      lf_pop_style_props();
    }
    lf_push_font(&textfont);
    LfUIElementProps props = lf_get_theme().text_props;
    props.margin_top = 4.0;
    props.margin_left = 5.0f;
    lf_push_style_props(props);

    float descptr_x = lf_get_ptr_x();
    lf_text(entry->desc);

    lf_set_ptr_x_absolute(descptr_x);
    lf_set_ptr_y_absolute(lf_get_ptr_y() + textfont.font_size);
    props.text_color = (LfColor){150, 150, 150, 255};
    lf_push_style_props(props);
    lf_text(entry->date);
    lf_pop_style_props();
    lf_pop_font();

    lf_next_line();
    
    renderedcount++;  
  }

  if(!renderedcount) {
    lf_text("There is no task here.");
  }

  lf_div_end();
    
}


static void rendernewtask() {
  lf_push_font(&titlefont);
  {
    LfUIElementProps props = lf_get_theme().text_props;
    props.margin_bottom = 15.0f;
    lf_push_style_props(props);
    lf_text("Add a new task");
    lf_pop_font();
  }

  lf_next_line();

  {
    lf_push_font(&textfont);
    lf_text("Description");
    lf_pop_font();

    lf_next_line();
    LfUIElementProps props = lf_get_theme().inputfield_props;
    props.padding = 15.0f;
    props.color = lf_color_from_zto((vec4s){0.05f, 0.05f, 0.05f, 1.0f});
    props.corner_radius = 11;
    props.text_color = LF_WHITE;  
    props.border_width = 1.0f;
    props.border_color = new_task_input.selected ? LF_WHITE : (LfColor){170, 170, 170, 255};
    props.corner_radius = 2.5f;
    props.margin_bottom = 10.f;
    lf_push_style_props(props);
    lf_input_text(&new_task_input);
    lf_pop_style_props();
  }

  lf_next_line();

  static int32_t selected_priority = -1;
  {
    lf_push_font(&textfont);
    lf_text("Priority");
    lf_pop_font();

    lf_next_line();
    static const char* items[3] = {
      "Low",
      "Medium",
      "High"
    };
    static bool opened = false;
    LfUIElementProps props = lf_get_theme().button_props;
    props.color = (LfColor){70, 70, 70, 255};
    props.text_color = LF_WHITE;  
    props.border_width = 0.0f; props.corner_radius = 5.0f;
    lf_push_style_props(props);
    lf_dropdown_menu(items, "Priority", 3, 200, 80, &selected_priority, &opened);
    lf_pop_style_props();
  }

  {
    bool form_complete = (strlen(new_task_input_buf) && selected_priority != -1);
    const char * text = "Add";
    const float width = 150.0f;

    LfUIElementProps props = lf_get_theme().button_props;
    props.margin_left = 0.0f; props.margin_right = 0.0f; 
    props.corner_radius = 5.0f; props.border_width = 0.0f;
    props.color = !form_complete ? (LfColor){80, 80, 80, 255} : (LfColor){65, 177, 204, 255};
    lf_push_style_props(props);
    lf_set_line_should_overflow(false);
    lf_set_ptr_x_absolute(winw - (width + props.padding * 2.0f) - WIN_MARGIN);
    lf_set_ptr_y_absolute(winh - (lf_button_dimension(text).y + props.padding * 2.0f) - WIN_MARGIN);
    if(lf_button_fixed(text, width, -1) == LF_CLICKED && form_complete) {
      task_entry* entry = (task_entry*)malloc(sizeof(*entry));
      entry->priority = selected_priority;
      entry->completed = false;
      entry->date = get_command_output("date +\"%d.%m.%Y, %H:%M\"");

      char* new_desc = malloc(strlen(new_task_input_buf));
      strcpy(new_desc, new_task_input_buf);
      
      entry->desc = new_desc;
      entries[numentries++] = entry;
      memset(new_task_input_buf, 0, 512);
    }
    lf_set_line_should_overflow(true);
    lf_pop_style_props();
  }

  lf_next_line();

  {
    LfUIElementProps props = lf_get_theme().button_props;
    props.color = LF_NO_COLOR; props.border_width = 0.0f;
    props.padding = 0.0f; props.margin_left = 0.0f; props.margin_top = 0.0f;
    props.margin_right = 0.0f; props.margin_bottom = 0.0f;
    lf_push_style_props(props);
    lf_set_line_should_overflow(false);
    LfTexture backbutton = (LfTexture){.id = backtexture.id, .width = 20, .height = 40};
    lf_set_ptr_y_absolute(winh - backbutton.height - WIN_MARGIN * 2.0f);
    lf_set_ptr_x_absolute(WIN_MARGIN);

    if(lf_image_button(backbutton) == LF_CLICKED) {
      current_tab = TAB_DASHBOARD;
    }
    lf_set_line_should_overflow(true);
    lf_pop_style_props();
  }

}


int main() {
  glfwInit();

  GLFWwindow* window = glfwCreateWindow(winw, winh, "Todo", NULL, NULL);

  glfwMakeContextCurrent(window);

  lf_init_glfw(winw, winh, window);



  LfTheme theme = lf_get_theme();
  theme.div_props.color = LF_NO_COLOR;
  lf_set_theme(theme);


  titlefont = lf_load_font("./fonts/redhat/RedHatText-Bold.ttf", 40);
  textfont = lf_load_font("./fonts/redhat/RedHatText-Regular.ttf", 20);

  removetexture = lf_load_texture("./icons/remove.png", true, LF_TEX_FILTER_LINEAR);
  backtexture = lf_load_texture("./icons/back.png", true, LF_TEX_FILTER_LINEAR);



  new_task_input = (LfInputField){
    .width = 400,
    .buf = new_task_input_buf,
    .buf_size = 512,
    .placeholder = "What is there to do?"
  };


  for(uint32_t i = 0; i < 5; i++) {
    
  }
  

  while(!glfwWindowShouldClose(window)) {
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(0.05f, 0.05f, 0.05f, 1.0f);

    lf_begin();

    lf_div_begin(((vec2s){WIN_MARGIN, WIN_MARGIN}),
                  ((vec2s){winw - WIN_MARGIN * 2.0F, winh - WIN_MARGIN * 2.0F}), 
                  true);


    switch (current_tab) {
      case TAB_DASHBOARD: {
        rendertopbar();
        lf_next_line();
        renderfilters();
        lf_next_line();
        renderentries();
        break;
      }
      case TAB_NEW_TASK: {
        rendernewtask();
      }
    }
    

    

    lf_div_end();
    lf_end();

    glfwPollEvents();
    glfwSwapBuffers(window);
  }


  lf_free_font(&titlefont);

  glfwDestroyWindow(window);
  glfwTerminate();


  return 0;

}
