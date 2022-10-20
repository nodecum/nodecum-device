#include "shell_parse.h"
#include "strbuf.h"
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(shell_parse, LOG_LEVEL_DBG);

/**
  Parsing the shell output
  ------------------------
  We characterize our approach
  - the shell prompt followed by a space is the indicator for
    a following command and or alternatives in the next,
    this line is the command line
  - alternatives start on a new line with spaces imediatly after
    the command line
  - all other text is output
  
*/

extern size_t shell_parse( const char* data, size_t length, 
			   struct shell_parse_t *p)
{
  int j = 0;
  for(; j < length; ++j) {
    // check the current character
    char c = data[j];
    // we save the last vt state if it is one of .. and differs to the
    // last state
    if( p->vt == Separator || p->vt == Character || p->vt == Newline) {
      if( p->vt_ == p->vt ) { p->vt_ = p->vt__; }
      else {
	if( p->vt__ != p->vt_) p->vt__ = p->vt_;
	p->vt_ = p->vt;
      }
    }
    if( c == 0x1b) { p->vt = Escape; continue; }
    if( p->vt == Escape && c == '[') { p->vt = EscapeBracket; continue; }
    if( p->vt == EscapeBracket ) {
      p->vt = Character;
      if( c == 'm' ) continue; // MODESOFF 
      if( c >= '0' && c <= '9') { p->vt = EscapeNumArg; continue; }
      continue;
    }
    if( p->vt == EscapeNumArg ) {
      if( c >= '0' && c <= '9')	continue;
      if( c == 'C' || c == 'D' ) { p->vt = Separator; continue; } // DIRECTION
      p->vt = Character; continue;
    }
    if( c == ' ') { p->vt = Separator; continue; }
    if( c == '\r' || c == '\n' ) { p->vt = Newline; continue; }
    // test for prompt
    if( (p->vt == Newline && p->prompt_i == 0) || p->ct == Prompt ) {      
      const char c_ = p->prompt[ p->prompt_i];
      // entire prompt has matched
      if( c_ == '\0') {
	LOG_INF("prompt: vt__=%d,vt_=%d,vt=%d", p->vt__, p->vt_, p->vt);
	// we got a newline followed by separator with no content before
	if( p->vt_ == Newline && p->vt == Separator) {
	  // Command line has ended and next line started with separator
	  // this means that we read in alternatives now
	  p->alt->size=0; p->ct = Alt;
	} else /*if( p->vt == Separator)*/ {
	  // change to Cmd context 
	  p->cmd->size=0; p->ct = Cmd; 
	}
	p->prompt_i = 0;
      } else if( c_ == c) {
	// charater matched, advance to next
	p->ct = Prompt; ++(p->prompt_i);
	p->vt = Character;
      } else {
	// character mismatch, write already matched chars to
	// Out Buffer and change to Out context
	// the mismatched char will be read in the Out context handling
	size_t i = 0;
	while( i < p->prompt_i) {
	  p->out->buffer[ p->out->size] = p->prompt[ i++];
	  STR_BUF_INC( p->out);
	}
	p->prompt_i = 0;
	p->ct = Out;
      }
    }
    if( p->ct == Cmd && p->vt == Newline) {
      // new line was not followed by separtor, this means output
      p->out->size=0; p->ct=Out;
    }
    // fill the buffers accordingly to their context
    if( p->ct == Cmd) {
      if( p->vt == Separator && p->cmd->size > 0 ) {
	p->cmd->buffer[ p->cmd->size] = ' '; STR_BUF_INC( p->cmd);
      }
      p->cmd->buffer[ p->cmd->size] = c; STR_BUF_INC( p->cmd);
    } else if( p->ct == Out) {
      if( p->vt == Separator && p->out->size > 0 ) {
	p->out->buffer[ p->out->size] = ' '; STR_BUF_INC( p->out);
      }
      p->out->buffer[ p->out->size] = c; STR_BUF_INC( p->out);
    } else if( p->ct == Alt) {
      if( p->vt == Separator && p->alt->size > 0 ) {
	p->alt->buffer[ p->alt->size] = ' '; STR_BUF_INC( p->alt);
      }
      p->alt->buffer[ p->alt->size] = c; STR_BUF_INC( p->alt);
    }
    p->vt = Character; // we can overwrite this now
  }
  p->cmd->buffer[ p->cmd->size] = '\0';
  p->alt->buffer[ p->alt->size] = '\0';
  p->out->buffer[ p->out->size] = '\0';
  return j;
}

