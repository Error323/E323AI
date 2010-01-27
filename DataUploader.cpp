#include "DataUploader.h"
#include <iostream>

/* Curl C */
#include <curl/curl.h>

CDataUploader::CDataUploader(const std::string &url)
{
	curl_global_init(CURL_GLOBAL_ALL);
	handle = curl_easy_init();
	curl_easy_setopt(handle, CURLOPT_URL, url.c_str());

	post = last = NULL;
	headerlist = NULL;
}

CDataUploader::~CDataUploader()
{
	curl_easy_cleanup(handle);
	curl_formfree(post);
	curl_slist_free_all(headerlist);
	curl_global_cleanup();
}

void CDataUploader::AddStringVec(const std::string &key, const std::vector<std::string> &data)
{
	/* Make our key an array according to html */
	std::string htmlkey = key + "[]";

	for (unsigned int i = 0; i < data.size(); i++)
		AddString(htmlkey, data[i]);
}

void CDataUploader::AddString(const std::string &key, const std::string &val)
{
	curl_formadd(&post, &last, 
		CURLFORM_COPYNAME, key.c_str(), 
		CURLFORM_COPYCONTENTS, val.c_str(),
	CURLFORM_END);
}

void CDataUploader::AddFile(const std::string &local, const std::string &remote)
{
	curl_formadd(&post, &last,
		CURLFORM_COPYNAME, remote.c_str(),
		CURLFORM_FILE, local.c_str(),
	CURLFORM_END);
}

void CDataUploader::AddHeader(const std::string &header)
{
	headerlist = curl_slist_append(headerlist, header.c_str());
}

void CDataUploader::SendData()
{
	/* Fill in the submit field too, for correctness */
	curl_formadd(&post, &last,
		CURLFORM_COPYNAME, "submit",
		CURLFORM_COPYCONTENTS, "send",
	CURLFORM_END);

	/* Set the headerlist */
	if (headerlist != NULL)
		curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headerlist);

	/* State that we want to use HTTP POST */
	curl_easy_setopt(handle, CURLOPT_HTTPPOST, post);

	/* Execute the actual post */
	CURLcode result = curl_easy_perform(handle);

	/* result != CURL_OK */
	if (result != 0)
		/* Something went wrong */
		std::cout << "Uploading error: " << curl_easy_strerror(result) << std::endl;
}
