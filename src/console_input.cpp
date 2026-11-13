#include "../include/console_input.hpp"

#ifdef _WIN32
#include <windows.h>

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

static HANDLE g_console_handle		 = nullptr;
static DWORD g_console_mode			 = 0;
static bool g_console_mode_valid	 = false;
static HANDLE g_console_out_handle	 = nullptr;
static DWORD g_console_out_mode		 = 0;
static bool g_console_out_mode_valid = false;

static BOOL WINAPI console_ctrl_handler(DWORD)
{
	if(g_console_mode_valid && g_console_handle)
		SetConsoleMode(g_console_handle, g_console_mode);
	if(g_console_out_mode_valid && g_console_out_handle)
		SetConsoleMode(g_console_out_handle, g_console_out_mode);
	return FALSE;
}
#else
#include <poll.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>

static struct termios g_saved_termios;
static bool g_termios_valid = false;

static void restore_terminal()
{
	if(g_termios_valid)
	{
		::tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_saved_termios);
		g_termios_valid = false;
	}
}

static void restore_and_exit(int sig)
{
	restore_terminal();
	::_exit(128 + sig);
}
#endif

ConsoleInput::ConsoleInput()
{
#ifdef _WIN32
	HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
	DWORD mode	  = 0;

	if(handle != nullptr && handle != INVALID_HANDLE_VALUE && GetConsoleMode(handle, &mode))
	{
		DWORD new_mode = mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
		if(SetConsoleMode(handle, new_mode))
		{
			handle_				= handle;
			old_mode_			= mode;
			mode_valid_			= true;
			g_console_handle	= handle;
			g_console_mode		= mode;
			g_console_mode_valid = true;
		}
	}

	HANDLE out		 = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD out_mode	 = 0;

	if(out != nullptr && out != INVALID_HANDLE_VALUE && GetConsoleMode(out, &out_mode))
	{
		DWORD new_out_mode = out_mode | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
		if(SetConsoleMode(out, new_out_mode))
		{
			out_handle_				 = out;
			old_out_mode_			 = out_mode;
			out_mode_valid_			 = true;
			g_console_out_handle	 = out;
			g_console_out_mode		 = out_mode;
			g_console_out_mode_valid = true;
		}
	}

	if(mode_valid_ || out_mode_valid_)
		SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
#else
	if(::isatty(STDIN_FILENO))
	{
		struct termios t;
		if(::tcgetattr(STDIN_FILENO, &t) == 0)
		{
			g_saved_termios = t;
			g_termios_valid = true;

			t.c_lflag &= ~(ICANON | ECHO);
			t.c_iflag &= ~(ICRNL | IXON);
			t.c_cc[VMIN]	 = 0;
			t.c_cc[VTIME] = 0;

			if(::tcsetattr(STDIN_FILENO, TCSANOW, &t) == 0)
			{
				raw_ = true;

				struct sigaction sa {};
				sa.sa_handler = restore_and_exit;
				::sigemptyset(&sa.sa_mask);
				::sigaction(SIGINT, &sa, nullptr);
				::sigaction(SIGTERM, &sa, nullptr);
				::sigaction(SIGHUP, &sa, nullptr);
				::sigaction(SIGQUIT, &sa, nullptr);
			}
			else
			{
				g_termios_valid = false;
			}
		}
	}
#endif
}

ConsoleInput::~ConsoleInput()
{
#ifdef _WIN32
	if(mode_valid_ || out_mode_valid_)
		SetConsoleCtrlHandler(console_ctrl_handler, FALSE);
	if(mode_valid_)
	{
		if(handle_ != nullptr)
			SetConsoleMode((HANDLE)handle_, (DWORD)old_mode_);
		g_console_mode_valid = false;
	}
	if(out_mode_valid_)
	{
		if(out_handle_ != nullptr)
			SetConsoleMode((HANDLE)out_handle_, (DWORD)old_out_mode_);
		g_console_out_mode_valid = false;
	}
#else
	if(raw_)
		restore_terminal();
#endif
}

bool ConsoleInput::poll(uint8_t& byte)
{
	if(eof_)
		return false;

#ifdef _WIN32
	if(pending_pos_ < pending_len_)
	{
		byte = pending_[pending_pos_++];
		return true;
	}

	HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
	if(handle == nullptr || handle == INVALID_HANDLE_VALUE)
	{
		eof_ = true;
		return false;
	}

	if(mode_valid_)
	{
		for(;;)
		{
			if(WaitForSingleObject(handle, 0) != WAIT_OBJECT_0)
				return false;

			INPUT_RECORD record;
			DWORD count = 0;
			if(!ReadConsoleInputW(handle, &record, 1, &count) || count != 1)
				return false;

			if(record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown)
				continue;

			WCHAR wc = record.Event.KeyEvent.uChar.UnicodeChar;
			if(wc == 0)
				continue;

			char utf8[4];
			int len = WideCharToMultiByte(CP_UTF8, 0, &wc, 1, utf8, sizeof(utf8), nullptr, nullptr);
			if(len <= 0)
				continue;

			pending_len_ = len;
			for(int i = 0; i < len; i++)
				pending_[i] = (uint8_t)utf8[i];
			pending_pos_ = 1;
			byte		 = pending_[0];
			return true;
		}
	}

	DWORD available = 0;
	if(PeekNamedPipe(handle, nullptr, 0, nullptr, &available, nullptr) && available == 0)
		return false;

	unsigned char value = 0;
	DWORD count			= 0;
	if(!ReadFile(handle, &value, 1, &count, nullptr) || count == 0)
	{
		eof_ = true;
		return false;
	}

	byte = value;
	return true;
#else
	struct pollfd pfd {};
	pfd.fd	   = STDIN_FILENO;
	pfd.events = POLLIN;

	int result = ::poll(&pfd, 1, 0);
	if(result <= 0)
		return false;

	if(pfd.revents & (POLLERR | POLLNVAL))
		return false;

	if(!(pfd.revents & (POLLIN | POLLHUP)))
		return false;

	unsigned char value = 0;
	ssize_t count		= ::read(STDIN_FILENO, &value, 1);
	if(count == 1)
	{
		byte = value;
		return true;
	}

	if(count == 0)
		eof_ = true;

	return false;
#endif
}
