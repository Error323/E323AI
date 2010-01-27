#ifndef UPLOADER_HDR
#define UPLOADER_HDR

/* Standard C++ */
#include <string>
#include <vector>

/* Curl fwd declaration */
struct curl_httppost;
struct curl_slist;

class CDataUploader
{
	public:
		CDataUploader(const std::string&);
		~CDataUploader();

		/* Add a string, key/value pair */
		void AddString(const std::string&, const std::string&);

		/* Add a vector of strings */
		void AddStringVec(const std::string&, const std::vector<std::string>&);

		/* Add a file, key/value pair */
		void AddFile(const std::string&, const std::string&);

		/* Add custom headers */
		void AddHeader(const std::string&);

		/* Send all the data through HTTP POST */
		void SendData();

	private:
		void *handle;

		struct curl_httppost *post;
		struct curl_httppost *last;
		struct curl_slist    *headerlist;
};

#endif /* UPLOADER_HDR */
