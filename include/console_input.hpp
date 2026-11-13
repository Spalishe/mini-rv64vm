#pragma once
#include <cstdint>

class ConsoleInput
{
  public:
	ConsoleInput();
	~ConsoleInput();

	ConsoleInput(const ConsoleInput&) = delete;
	ConsoleInput& operator=(const ConsoleInput&) = delete;

	bool poll(uint8_t& byte);
	bool eof() const { return eof_; }

  private:
	bool eof_ = false;
#ifdef _WIN32
	void* handle_			   = nullptr;
	unsigned long old_mode_	 = 0;
	bool mode_valid_		   = false;
	void* out_handle_		   = nullptr;
	unsigned long old_out_mode_ = 0;
	bool out_mode_valid_	   = false;
	uint8_t pending_[4]		   = {};
	int pending_len_		   = 0;
	int pending_pos_		   = 0;
#else
	bool raw_ = false;
#endif
};
