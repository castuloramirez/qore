/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
 QoreURL.h
 
 Network functions and macros
 
 Qore Programming Language
 
 Copyright 2003 - 2012 David Nichols
 
 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
 
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.
 
 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _QORE_QOREURL_H

#define _QORE_QOREURL_H

//! helps with parsing URLs and provides access to URL components through Qore data structures
class QoreURL {
   private:
      //! private implementation of the class
      struct qore_url_private *priv;

      DLLLOCAL void zero();
      DLLLOCAL void reset();
      DLLLOCAL void parseIntern(const char *url);

      //! this function is not implemented; it is here as a private function in order to prohibit it from being used
      DLLLOCAL QoreURL(const QoreURL&);

      //! this function is not implemented; it is here as a private function in order to prohibit it from being used
      DLLLOCAL QoreURL& operator=(const QoreURL&);

   public:
      //! creates an empty structure
      /** @see QoreURL::parse()
       */
      DLLEXPORT QoreURL();

      //! parses the URL string passed
      /** you can check if the URL was valid by calling QoreURL::isValid() after this call
	  @param url the URL string to parse
       */
      DLLEXPORT QoreURL(const char *url);

      //! parses the URL string passed
      /** you can check if the URL was valid by calling QoreURL::isValid() after this call
	  @param url the URL string to parse
       */
      DLLEXPORT QoreURL(const class QoreString *url);

      //! frees all memory and destroys the structure
      DLLEXPORT ~QoreURL();

      //! parses the URL string passed
      /** If a url was already parsed previously, all memory is freed before parsing the new string.
	  You can check if the URL was valid by calling QoreURL::isValid() after this call
	  @param url the URL string to parse
       */
      DLLEXPORT int parse(const char *url);

      //! parses the URL string passed
      /** If a url was already parsed previously, all memory is freed before parsing the new string.
	  You can check if the URL was valid by calling QoreURL::isValid() after this call
	  @param url the URL string to parse
       */
      DLLEXPORT int parse(const class QoreString *url);

      //! returns true if the URL string parsed is valid
      /** @return true if the URL string parsed is valid
       */
      DLLEXPORT bool isValid() const;

      //! returns a hash of the parameters parsed, destructive: zeros out all elements, caller owns the reference count returned
      /** hash keys are:
	  - protocol
	  - path
	  - username
	  - password
	  - host
	  - port
	  .
	  each key is either a QoreStringNode or 0 except for port which is a QoreBigIntNode
	  @note the caller must call QoreHashNode::deref() manually on the value returned if it's not 0 (or use the ReferenceHolder helper class)
	  @return a hash of the parameters parsed
       */
      DLLEXPORT QoreHashNode *getHash();
 
      //! returns the hostname of the URL
      /** @return the hostname of the URL
       */
      DLLEXPORT const QoreString *getHost() const;

      //! returns the user name in the URL or 0 if none given
      /** @return the user name in the URL or 0 if none given
       */
      DLLEXPORT const QoreString *getUserName() const;

      //! returns the password in the URL or 0 if none given
      /** @return the password in the URL or 0 if none given
       */
      DLLEXPORT const QoreString *getPassword() const;

      //! returns the path component of the URL or 0 if none given
      /** @return the path component of the URL or 0 if none given
       */
      DLLEXPORT const QoreString *getPath() const;

      //! returns the protocol component of the URL or 0 if none given
      DLLEXPORT const QoreString *getProtocol() const;

      //! returns the port number given in the URL or 0 if none present
      /** @return the port number given in the URL or 0 if none present
       */
      DLLEXPORT int getPort() const;
      
      // the "take" methods return the char * pointers for the data
      // the caller owns the memory

      //! returns a pointer to the path (0 if none present), caller owns the memory returned
      /** if this function returns a non-zero pointer, the memory must be manually freed by the caller
	  @return a pointer to the path (0 if none present), caller owns the memory returned
       */
      DLLEXPORT char *take_path();

      //! returns a pointer to the username in the URL (0 if none present), caller owns the memory returned
      /** if this function returns a non-zero pointer, the memory must be manually freed by the caller
	  @return a pointer to the username (0 if none present), caller owns the memory returned
       */
      DLLEXPORT char *take_username();

      //! returns a pointer to the password in the URL (0 if none present), caller owns the memory returned
      /** if this function returns a non-zero pointer, the memory must be manually freed by the caller
	  @return a pointer to the password (0 if none present), caller owns the memory returned
       */
      DLLEXPORT char *take_password();

      //! returns a pointer to the hostname in the URL (0 if none present), caller owns the memory returned
      /** if this function returns a non-zero pointer, the memory must be manually freed by the caller
	  @return a pointer to the hostname (0 if none present), caller owns the memory returned
       */
      DLLEXPORT char *take_host();
};

#endif
