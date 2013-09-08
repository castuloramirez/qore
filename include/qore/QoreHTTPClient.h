/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  QoreHTTPClient.h

  Qore Programming Language

  Copyright (C) 2006 - 2013 QoreTechnologies
  
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

/* this file is deprecated; use QoreHttpClientObject instead
   this class is only kept for ABI compatibility
*/

#ifndef QORE_HTTP_CLIENT_H_
#define QORE_HTTP_CLIENT_H_

#include <qore/common.h>
#include <qore/AbstractPrivateData.h>
#include <qore/QoreThreadLock.h>
#include <qore/QoreSocket.h>
#include <qore/QoreHttpClientObject.h>

class Queue;

//! provides a way to communicate with HTTP servers using Qore data structures
/** thread-safe, uses QoreSocket for socket communication
 */
class QoreHTTPClient : public AbstractPrivateData {
private:
   //! private implementation of the class
   struct qore_qtc_private *priv;

   //! this function is not implemented; it is here as a private function in order to prohibit it from being used
   DLLLOCAL QoreHTTPClient(const QoreHTTPClient&);

   //! this function is not implemented; it is here as a private function in order to prohibit it from being used
   DLLLOCAL QoreHTTPClient& operator=(const QoreHTTPClient&);

protected:
   DLLEXPORT void lock();
   DLLEXPORT void unlock();

public:
   //! creates the QoreHTTPClient object
   DLLEXPORT QoreHTTPClient();

   //! destroys the object and frees all associated memory
   DLLEXPORT virtual ~QoreHTTPClient();

   //! set options with a hash, returns -1 if an exception was thrown, 0 for OK
   /** options are:
       - protocols: a hash where each key is a protocol name and the value must be set to a integer giving a port number or a hash having the following keys:
       - port: giving the port number
       - ssl: giving a boolean true or false value
       - max_redirects: sets the max_redirects option
       - default_port: sets the default port number
       - proxy: sets the proxy URL
       - url: sets the default connection URL
       - default_path: sets the default path
       - timeout: sets the connection or response packet timeout value in milliseconds
       - http_version: either "1.0" or "1.1" to set the default HTTP version to use
       - connect_timeout: an integer giving the timeout value for new socket connections in milliseconds
       @note this function is unlocked and designed only to be called with the constructor
       @param opts the options to set for the object
       @param xsink if an error occurs, the Qore-language exception information will be added here	  
       @return -1 if an exception was thrown, 0 for OK
   */
   DLLEXPORT int setOptions(const QoreHashNode *opts, ExceptionSink* xsink);

   //! sets the default port
   /** useful for c++ derived classes
    */
   DLLEXPORT void setDefaultPort(int prt);

   //! sets the default path
   /** useful for c++ derived classes
    */
   DLLEXPORT void setDefaultPath(const char *pth);

   //! returns the default path or 0 if none is set
   /** @since Qore 0.8.8
    */
   DLLEXPORT const char* getDefaultPath() const;

   //! returns the current connection path or 0 if none is set
   /** @since Qore 0.8.8
    */
   DLLEXPORT const char* getConnectionPath() const;

   //! adds a protocol
   /** useful for c++ derived classes
    */
   DLLEXPORT void addProtocol(const char *prot, int port, bool ssl = false);

   //! sets the connection and response packet timeout value in milliseconds
   DLLEXPORT void setTimeout(int to);

   //! returns the connection and response packet timeout value in milliseconds
   DLLEXPORT int getTimeout() const;

   //! sets the default encoding for the object
   DLLEXPORT void setEncoding(const QoreEncoding *qe);

   //! returns the default encoding for the object
   DLLEXPORT const QoreEncoding *getEncoding() const;
      
   //! sets the http version from a string
   /**
      @param version either "1.0" or "1.1"
      @param xsink if an error occurs, the Qore-language exception information will be added here	  
      @return -1 if an exception was thrown, 0 for OK
   */
   DLLEXPORT int setHTTPVersion(const char* version, ExceptionSink* xsink);

   //! returns the http version as a string (either "1.0" or "1.1")
   /**
      @return the http version as a string (either "1.0" or "1.1")
   */
   DLLEXPORT const char* getHTTPVersion() const;

   //! sets or clears HTTP 1.1 protocol compliance
   /**
      @param h11 if true sets HTTP 1.1 protocol compliance, if false set 1.0
   */
   DLLEXPORT void setHTTP11(bool h11);

   //! returns true if HTTP 1.1 protocol compliance has been set
   DLLEXPORT bool isHTTP11() const;

   //! sets the connection URL
   /** @param url the URL to use for connection parameters
       @param xsink if an error occurs, the Qore-language exception information will be added here	  
       @return -1 if an exception was thrown, 0 for OK
   */
   DLLEXPORT int setURL(const char *url, ExceptionSink *xsink);

   //! returns the connection parameters as a URL, caller owns the reference count returned
   /**
      @return the connection parameters as a URL, caller owns the reference count returned
   */
   DLLEXPORT QoreStringNode *getURL();

   //! sets the username and password for the connection
   /** @param user the username to set
       @param pass the password to set
       @note setURL() will overwrite any settings set here
   */
   DLLEXPORT void setUserPassword(const char *user, const char *pass);

   //! clears the username and password for the connection
   DLLEXPORT void clearUserPassword();

   //! sets the proxy URL
   /** @param proxy the URL to use for connection to the proxy
       @param xsink if an error occurs, the Qore-language exception information will be added here	  
       @return -1 if an exception was thrown, 0 for OK
   */
   DLLEXPORT int setProxyURL(const char *proxy, ExceptionSink *xsink);

   //! returns the proxy connection parameters as a URL (or 0 if there is none), caller owns the reference count returned
   /**
      @return the proxy connection parameters as a URL, caller owns the reference count returned
   */
   DLLEXPORT QoreStringNode *getProxyURL();

   //! clears the proxy URL
   DLLEXPORT void clearProxyURL();

   //! sets the username and password for the proxy connection
   /** @param user the username to set
       @param pass the password to set
       @note these settings will only take effect if a proxy URL is set, so it only makes sense to call this function after setProxyURL(); also setProxyURL() will overwrite any settings here.
   */
   DLLEXPORT void setProxyUserPassword(const char *user, const char *pass);

   //! clears the username and password for the proxy connection
   DLLEXPORT void clearProxyUserPassword();

   //! sets the SSL flag for use in the next connection
   DLLEXPORT void setSecure(bool is_secure);

   //! returns the SSL connection parameter flag
   DLLEXPORT bool isSecure() const;

   //! sets the SSL flag for use in the next connection to the proxy
   DLLEXPORT void setProxySecure(bool is_secure);

   //! returns the SSL proxy connection parameter flag
   DLLEXPORT bool isProxySecure() const;

   //! sets the max_redirects option
   DLLEXPORT void setMaxRedirects(int max);

   //! returns the value of the max_redirects option
   DLLEXPORT int getMaxRedirects() const;

   //! returns the peer certificate verification code if an SSL connection is in progress
   DLLEXPORT long verifyPeerCertificate();

   //! returns the name of the SSL Cipher for the currently-connected control connection, or 0 if there is none
   /**
      @return the name of the SSL Cipher for the currently-connected control connection, or 0 if there is none
   */
   DLLEXPORT const char *getSSLCipherName();

   //! returns the version string of the SSL Cipher for the currently-connected control connection, or 0 if there is none
   /**
      @return the version string of the SSL Cipher for the currently-connected control connection, or 0 if there is none
   */
   DLLEXPORT const char *getSSLCipherVersion();

   //! opens a connection and returns a code giving the result
   /** @return -1 if an exception was thrown, 0 for OK
    */
   DLLEXPORT int connect(ExceptionSink *xsink);

   //! disconnects from the remote server
   DLLEXPORT void disconnect();
      
   //! sends a message to the remote server and returns the entire response as a hash, caller owns the QoreHashNode reference returned
   /** possible errors: method not recognized, redirection errors, socket communication errors, timeout errors
       @param meth the HTTP method name to send
       @param path the path string to send in the header
       @param headers a hash of headers to add to the message
       @param data optional data to send (may be 0)
       @param size the byte length of the data to send (if this is 0 then no data is sent)
       @param getbody if true then a body will be read even if there is no "Content-Length:" header
       @param info if not 0 then additional information about the HTTP communication will be added to the hash (key-value pairs), keys "headers", and optionally "redirect-#", "redirect-message-#" (where # is substituted with the redirect sequence number), and "chunked" (boolean, present only if the response was chunked)
       @param xsink if an error occurs, the Qore-language exception information will be added here	  
       @return the entire response as a hash, caller owns the QoreHashNode reference returned (0 if there was an error)
   */
   DLLEXPORT QoreHashNode *send(const char *meth, const char *path, const QoreHashNode *headers, const void *data, unsigned size, bool getbody, QoreHashNode *info, ExceptionSink *xsink);

   //! sends an HTTP "GET" method and returns the value of the message body returned, the caller owns the AbstractQoreNode reference returned
   /** if you need to get all the headers received, then use QoreHTTPClient::send() instead
       @param path the path string to send in the header
       @param headers a hash of headers to add to the message
       @param info if not 0 then additional information about the HTTP communication will be added to the hash (key-value pairs), keys "headers", and optionally "redirect-#", "redirect-message-#" (where # is substituted with the redirect sequence number), and "chunked" (boolean, present only if the response was chunked)
       @param xsink if an error occurs, the Qore-language exception information will be added here	  
       @return the body of the response message, caller owns the QoreHashNode reference returned (0 if there was an error or no body returned)
   */
   DLLEXPORT AbstractQoreNode *get(const char *path, const QoreHashNode *headers, QoreHashNode *info, ExceptionSink *xsink);

   //! sends an HTTP "HEAD" method and returns the headers returned, the caller owns the QoreHashNode reference returned
   /** @param path the path string to send in the header
       @param headers a hash of headers to add to the message
       @param info if not 0 then additional information about the HTTP communication will be added to the hash (key-value pairs), keys "headers", and optionally "redirect-#", "redirect-message-#" (where # is substituted with the redirect sequence number), and "chunked" (boolean, present only if the response was chunked)
       @param xsink if an error occurs, the Qore-language exception information will be added here	  
       @return the entire response as a hash, caller owns the QoreHashNode reference returned (0 if there was an error)
   */
   DLLEXPORT QoreHashNode *head(const char *path, const QoreHashNode *headers, QoreHashNode *info, ExceptionSink *xsink);

   //! sends an HTTP "POST" message to the remote server and returns the message body of the response, caller owns the AbstractQoreNode reference returned
   /** possible errors: method not recognized, redirection errors, socket communication errors, timeout errors
       @param path the path string to send in the header
       @param headers a hash of headers to add to the message
       @param data optional data to send (should not be 0 for a POST)
       @param size the byte length of the data to send (if this is 0 then no data is sent)
       @param info if not 0 then additional information about the HTTP communication will be added to the hash (key-value pairs), keys "headers", and optionally "redirect-#", "redirect-message-#" (where # is substituted with the redirect sequence number), and "chunked" (boolean, present only if the response was chunked)
       @param xsink if an error occurs, the Qore-language exception information will be added here	  
       @return the body of the response message, caller owns the QoreHashNode reference returned (0 if there was an error or no body returned)
   */
   DLLEXPORT AbstractQoreNode *post(const char *path, const QoreHashNode *headers, const void *data, unsigned size, QoreHashNode *info, ExceptionSink *xsink);

   //! sets the value of a default header to send with every outgoing message
   /**
      @param header the name of the header to send
      @param val the string value to use in the HTTP header
   */
   DLLEXPORT void setDefaultHeaderValue(const char *header, const char *val);

   using AbstractPrivateData::deref;
   //! decrements the reference count and deletes the object when it reaches 0
   /**
      @param xsink if an error occurs, the Qore-language exception information will be added here
   */
   DLLEXPORT virtual void deref(ExceptionSink *xsink);

   //! sets the connect timeout in ms
   /**
      @param ms connect timeout in ms
   **/
   DLLEXPORT void setConnectTimeout(int ms);

   //! returns the connect timeout in ms, negative numbers mean no timeout
   /**
      @return the connect timeout in ms, negative numbers mean no timeout
   **/
   DLLEXPORT int getConnectTimeout() const;

   //! sets the TCP_NODELAY flag on the object
   /**
      This function will try to set the TCP_NODELAY flag immediately if the
      socket is connected, otherwise will it set a flag and the TCP_NODELAY
      option will be set on the next connection.  If an error occurs
      setting TCP_NODELAY on a connected socket, then this function will
      return a non-zero value, and errno will be set
      @param nodelay 0=turn off TCP_NODELAY, non-zero=turn on TCP_NODELAY
      @return 0=OK, non-zero means an error occured, errno is set
   */
   DLLEXPORT int setNoDelay(bool nodelay);

   //! sets the event queue, must be already referenced before call
   DLLEXPORT void setEventQueue(Queue *cbq, ExceptionSink *xsink);

   //! returns the value of the TCP_NODELAY flag on the object
   DLLEXPORT bool getNoDelay() const;

   //! returns the connection status of the object
   DLLEXPORT bool isConnected() const;

   //! returns peer information for a connected socket
   /** if the socket is not connected, a Qore-language exception is thrown
       @param xsink if an error occurs, the Qore-language exception information will be added here
       @return a hash with the following keys:
       - \c hostname: the hostname of the remote end (if known or appropriate for the socket type)
       - \c hostname_desc: a descriptive string for the remote hostname (including the socket type - ie "ipv6[host]")
       - \c address: the address of the remote end - for UNIX sockets this is the file path
       - \c address_desc: a descriptive string for the remote address
       - \c port: the port number if known
       - \c family: the address family (ie AF_INET, AF_INET6, AF_UNIX, ...)
       - \c familystr: a string description of the address family ("ipv4", "ipv6", etc)
   */
   DLLEXPORT QoreHashNode* getPeerInfo(ExceptionSink* xsink) const;

   //! returns information for the current socket; the socket must be open
   /** if the socket is not open, a Qore-language exception is thrown
       @param xsink if an error occurs, the Qore-language exception information will be added here
       @return a hash with the following keys:
       - \c hostname: the hostname for the local interface (if known or appropriate for the socket type)
       - \c hostname_desc: a descriptive string for the local hostname (including the socket type - ie "ipv6[host]")
       - \c address: the address of the local interface - for UNIX sockets this is the file path
       - \c address_desc: a descriptive string for the local interface
       - \c port: the port number if known
       - \c family: the address family (ie AF_INET, AF_INET6, AF_UNIX, ...)
       - \c familystr: a string description of the address family ("ipv4", "ipv6", etc)
   */
   DLLEXPORT QoreHashNode* getSocketInfo(ExceptionSink* xsink) const;

   DLLLOCAL static void static_init();

   DLLLOCAL void cleanup(ExceptionSink *xsink);
};

#endif
