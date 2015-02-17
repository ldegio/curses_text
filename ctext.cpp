#include "ctext.h"

const ctext_config config_default = {
  .m_tabstop = CTEXT_DEFAULT_TABSTOP,
  .m_scrollback = CTEXT_DEFAULT_SCROLLBACK,
  .m_bounding_box = CTEXT_DEFAULT_BOUNDING_BOX,
  .m_do_wrap = CTEXT_DEFAULT_DO_WRAP,
  .m_append_top = CTEXT_DEFAULT_APPEND_TOP,
  .m_scroll_on_append = CTEXT_DEFAULT_APPEND_TOP,
  .m_on_event = CTEXT_DEFAULT_ON_EVENT
};

char really_large_buffer[64000] = {0};

ctext::ctext(WINDOW *win = 0, ctext_config *config = 0)
{
  this->m_win = win;
  
  if(config) 
  {
    memcpy(&this->m_config, config, sizeof(ctext_config));
  } 
  else 
  {
    memcpy(&this->m_config, config_default, sizeof(ctext_config));
  }

  this->m_pos_x = 0;
  this->m_pos_y = 0;

  this->m_max_x = 0;
  this->m_max_y = 0;
}

int8_t ctext::set_config(ctext_config *config)
{
  memcpy(&this->m_config, config, sizeof(ctext_config));
  return this->render();
}

int8_t ctext::get_config(ctext_config *config)
{
  memcpy(&config, &this->m_config, sizeof(ctext_config));
  return 0;
}

int8_t ctext::attach_curses_window(WINDOW *win)
{
  this->m_win = win;
  return this->render();
}

size_t ctext::clear(size_t amount = 0)
{
  if(amount == 0) 
  {
    this->m_buffer.clear();
  }
  else
  {
    this->m_buffer.erase(this->m_buffer.begin(), this->m_buffer.begin() + amount);
  }

  if (this->m_config.m_on_event)
  {
    this->m_config.m_on_event(this, ctext_event::CLEAR);
  }

  return this->render();
}

int8_t ctext::direct_scroll(size_t x, size_t y)
{
  if(this->m_config.m_bounding_box) 
  {
    x = max(0, x);
    y = max(0, y);
    x = min(x, this->m_max_x);
    y = min(y, this->m_max_y);
  }

  this->m_pos_x = x;
  this->m_pos_y = y;

  if (this->m_config.m_on_event)
  {
    this->m_config.m_on_event(this, ctext_event::SCROLL);
  }

  return 0;
}

int8_t ctext::scroll_to(size_t x, size_t y)
{
  this->direct_scroll(x, y);
  return this->render();
}

int8_t ctext::get_offset(size_t*x, size_t*y)
{
  *x = this->m_pos_x;
  *y = this->m_pos_y;

  return 0;
}

int8_t get_size(size_t*x, size_t*y)
{
  *x = this->m_max_x;
  *y = this->m_max_y;

  return 0;
}

int8_t ctext::up(size_t amount = 1) 
{
  return this->down(-amount);
}

int8_t ctext::down(size_t amount = 1) 
{
  return this->scroll_to(this->m_pos_x, this->m_pos_y + amount);
}

int8_t ctext::left(size_t amount = 1) 
{
  return this->right(-amount);
}

int8_t ctext:right(size_t amount = 1) 
{
  return this->scroll_to(this->m_pos_x + amount, this->m_pos_y);
}

int8_t ctext::printf(const char*format, ...) 
{
  this->render();
}

void ctext::get_win_size() 
{
  int32_t width = 0, height = 0;

  if(this->m_win)
  {
    getmaxyx(this->m_win, height, width);
  }
  this->m_win_width = width;
  this->m_win_height = height;
}

int8_t ctext::rebuf()
{
  this->get_win_size();

  // The actual scroll back, which is what
  // people expect in this feature is the 
  // configured scrollback + the window height.
  size_t actual_scroll_back = this->m_win_height + this->m_config.m_scrollback;

  if(this->m_buffer.size() > actual_scroll_back)
  {
    this->m_buffer.erase(this->m_buffer.begin(), this->m_buffer().size() - actual_scroll_back);
  }
  
  this->m_max_x = 0;  
  // Now unfortunately we have to do a scan over everything in N time to find
  // the maximum length string --- but only if we care about the bounding
  // box
  if(this->m_config.m_bounding_box)
  {
    for(ctext_buffer::const_iterator it = this->m_buffer.begin(); it != this->m_buffer.end(); it++) 
    {
      this->m_max_x = max(this->m_max_x, (*it).size());
    }
  }
 
  // this is practically free so we'll just do it.
  this->m_max_y = this->m_buffer().size();
  
  // Since we've changed the bounding box of the content we have to
  // issue a rescroll on exactly our previous parameters. This may
  // force us inward or may retain our position.
  return this->direct_scroll(this->m_pos_x, this->m_pos_y);
}

int8_t ctext::printf(const char*format, ...)
{
  va_list args;
  va_start(args, fmt);
  vsprintf(really_large_buffer, args);
  va_end(args);

  this->m_buffer.push_back(really_large_buffer);

  if (this->m_config.m_on_event)
  {
    this->m_config.m_on_event(this, ctext_event::DATA);
  }
  
  // Since we are adding content we need to see if we are
  // to force on scroll.
  if(this->m_config.m_scroll_on_append)
  {
    this->get_win_size();
    // now we force it.
    this->direct_scroll(this->m_buffer.size() - this->m_win_height, 0);
  }

  return this->render();
}

int8_t ctext::render() 
{
  // Calculate the bounds of everything first.
  this->rebuf();

  if(!this->m_win)
  {
    // Not doing anything without a window.
    return -1;
  }

  this->get_win_size();

  //
  // By this time, if we are bounded by a box,
  // it has been accounted for.
  //
  // Really our only point of interest is
  // whether we need to append to bottom
  // or append to top.
  //
  // We will assume that we can
  // populate the window quick enough
  // to avoid linear updating or paging.
  //  ... it's 2015 after all.
  //
  werase(this->m_win);

  // Regardless of whether this is append to top
  // or bottom we generate top to bottom.

  size_t start_char = max(0, this->m_pos_y);
  size_t offset = start_char;
  // the endchar will be in the substr
  
  // We start as m_pos_x in our list and move up to
  // m_pos_x + m_win_height except in the case of 
  // wrap around.  Because of this special case,
  // we compute when to exit slightly differently.
  //
  // This is the current line of output, which stays
  // below m_win_height
  size_t line = 0;

  // start at the beginning of the buffer.
  size_t index = this->m_pos_x;
  size_t directionality = +1;

  // if we are appending to the top then we start
  // at the end and change our directionality.
  if(this->m_config.m_append_top)
  {
    directionality = -1;
    index = this->m_pos_x + this->m_win_height;
  }

  while(line < this->m_win_height)
  {
    // Reset the offset.
    offset = start_char;

    index += directionality;
    if(index < this->m_max_y || index >= 0)
    {
      // We only index into the object if we have the
      // data to do so.
      wstring to_add = this->m_buffer[index].substr(start_char, this->m_win_width);
      mvwaddwstr(this->m_win, line, 0, to_add);

      // if we are wrapping, then we do that here.
      while(
          this->m_config.m_do_wrap && 

          // if our string still exhausts our entire width
          to_add.size() == this->m_win_width &&

          // and we haven't hit the bottom
          line < this->m_win_height
        )
      {
        // move our line forward
        line++;

        // and the start_char
        start_char += this->m_win_width;

        // substring into this character now at this advanced position
        to_add = this->m_buffer[index].substr(start_char, this->m_win_width);
        
        // and add it to the screen
        mvwaddwstr(this->m_win, line, 0, to_add);
      }
    }
    line++;
  }

  wrefresh(this->m_win);
}
