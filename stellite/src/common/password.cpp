// Copyright (c) 2014-2017, The Stellite Project
// 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
// 
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

#include "password.h"

#include <iostream>
#include <memory.h>
#include <stdio.h>

#if defined(_WIN32)
#include <io.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

#ifdef HAVE_READLINE
  #include "readline_buffer.h"
#endif

namespace
{
#if defined(_WIN32)
  bool is_cin_tty() noexcept
  {
    return 0 != _isatty(_fileno(stdin));
  }

  bool read_from_tty(std::string& pass)
  {
    static constexpr const char BACKSPACE = 8;

    HANDLE h_cin = ::GetStdHandle(STD_INPUT_HANDLE);

    DWORD mode_old;
    ::GetConsoleMode(h_cin, &mode_old);
    DWORD mode_new = mode_old & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
    ::SetConsoleMode(h_cin, mode_new);

    bool r = true;
    pass.reserve(tools::password_container::max_password_size);
    while (pass.size() < tools::password_container::max_password_size)
    {
      DWORD read;
      char ch;
      r = (TRUE == ::ReadConsoleA(h_cin, &ch, 1, &read, NULL));
      r &= (1 == read);
      if (!r)
      {
        break;
      }
      else if (ch == '\n' || ch == '\r')
      {
        std::cout << std::endl;
        break;
      }
      else if (ch == BACKSPACE)
      {
        if (!pass.empty())
        {
          pass.back() = '\0';
          pass.resize(pass.size() - 1);
          std::cout << "\b \b";
        }
      }
      else
      {
        pass.push_back(ch);
        std::cout << '*';
      }
    }

    ::SetConsoleMode(h_cin, mode_old);

    return r;
  }

#else // end WIN32 

  bool is_cin_tty() noexcept
  {
    return 0 != isatty(fileno(stdin));
  }

  int getch() noexcept
  {
    struct termios tty_old;
    tcgetattr(STDIN_FILENO, &tty_old);

    struct termios tty_new;
    tty_new = tty_old;
    tty_new.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &tty_new);

    int ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &tty_old);

    return ch;
  }

  bool read_from_tty(std::string& aPass)
  {
    static constexpr const char BACKSPACE = 127;

    aPass.reserve(tools::password_container::max_password_size);
    while (aPass.size() < tools::password_container::max_password_size)
    {
      int ch = getch();
      if (EOF == ch)
      {
        return false;
      }
      else if (ch == '\n' || ch == '\r')
      {
        std::cout << std::endl;
        break;
      }
      else if (ch == BACKSPACE)
      {
        if (!aPass.empty())
        {
          aPass.back() = '\0';
          aPass.resize(aPass.size() - 1);
          std::cout << "\b \b";
        }
      }
      else
      {
        aPass.push_back(ch);
        std::cout << '*';
      }
    }

    return true;
  }

#endif // end !WIN32

  void clear(std::string& pass) noexcept
  {
    //! TODO Call a memory wipe function that hopefully is not optimized out
    pass.replace(0, pass.capacity(), pass.capacity(), '\0');
    pass.clear();
  }

  bool read_from_tty(const bool verify, const char *message, std::string& pass1, std::string& pass2)
  {
    while (true)
    {
      if (message)
        std::cout << message <<": ";
      if (!read_from_tty(pass1))
        return false;
      if (verify)
      {
        std::cout << "Confirm Password: ";
        if (!read_from_tty(pass2))
          return false;
        if(pass1!=pass2)
        {
          std::cout << "Passwords do not match! Please try again." << std::endl;
          clear(pass1);
          clear(pass2);
        }
        else //new password matches
          return true;
      }
      else
        return true;
        //No need to verify password entered at this point in the code
    }

    return false;
  }

  bool read_from_file(std::string& pass)
  {
    pass.reserve(tools::password_container::max_password_size);
    for (size_t i = 0; i < tools::password_container::max_password_size; ++i)
    {
      char ch = static_cast<char>(std::cin.get());
      if (std::cin.eof() || ch == '\n' || ch == '\r')
      {
        break;
      }
      else if (std::cin.fail())
      {
        return false;
      }
      else
      {
        pass.push_back(ch);
      }
    }
    return true;
  }

} // anonymous namespace

namespace tools 
{
  // deleted via private member
  password_container::password_container() noexcept : m_password() {}
  password_container::password_container(std::string&& password) noexcept
    : m_password(std::move(password)) 
  {
  }

  password_container::~password_container() noexcept
  {
    clear(m_password);
  }

  boost::optional<password_container> password_container::prompt(const bool verify, const char *message)
  {
#ifdef HAVE_READLINE
    rdln::suspend_readline pause_readline;
#endif
    password_container pass1{};
    password_container pass2{};
    if (is_cin_tty() ? read_from_tty(verify, message, pass1.m_password, pass2.m_password) : read_from_file(pass1.m_password))
      return {std::move(pass1)};

    return boost::none;
  }

  boost::optional<login> login::parse(std::string&& userpass, bool verify, const char* message)
  {
    login out{};
    password_container wipe{std::move(userpass)};

    const auto loc = wipe.password().find(':');
    if (loc == std::string::npos)
    {
      auto result = tools::password_container::prompt(verify, message);
      if (!result)
        return boost::none;

      out.password = std::move(*result);
    }
    else
    {
      out.password = password_container{wipe.password().substr(loc + 1)};
    }

    out.username = wipe.password().substr(0, loc);
    return {std::move(out)};
  }
} 
