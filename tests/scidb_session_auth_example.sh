#!/bin/bash

# Example illustrating SciDB authentication over TLS using the default user
# name 'root' and password 'Paradigm4'. We don't run this as an automated test,
# this is just an example that shows how to do this.

PASSWORD=Paradigm4
USER=root

host=localhost
port=8083

api_url="https://${host}:${port}"
curl_cmd="curl --silent --show-error --insecure"


id=$($curl_cmd "$api_url/new_session?user=${USER}&password=${PASSWORD}")
$curl_cmd "$api_url/execute_query?id=${id}&query=list('instances')&save=dcsv"
$curl_cmd "$api_url/read_lines?id=${id}&n=0"
$curl_cmd "$api_url/release_session?id=${id}"
echo


# Test the new prefix option to issue multiple commands in a single connection
# A namespace example. Part 1: Create namespace 'cazart'
id=$($curl_cmd "$api_url/new_session?user=${USER}&password=${PASSWORD}")
$curl_cmd "$api_url/execute_query?id=${id}&query=create_namespace('cazart')&release=1" > /dev/null
echo "create_namespace('cazart')"


# Add an array to the cazart namespace
id=$($curl_cmd "$api_url/new_session?user=${USER}&password=${PASSWORD}")
$curl_cmd "$api_url/execute_query?id=${id}&prefix=set_namespace('cazart')&query=store(list(),yikes)&release=1" > /dev/null


# context (list the contents of the 'cazart' namespace)
id=$($curl_cmd "$api_url/new_session?user=${USER}&password=${PASSWORD}")
$curl_cmd "$api_url/execute_query?id=${id}&prefix=set_namespace('cazart')&query=list()&save=dcsv" > /dev/null
$curl_cmd "$api_url/read_lines?id=${id}&n=0"
$curl_cmd "$api_url/release_session?id=${id}"


#It's good to clean up after yourself
id=$($curl_cmd "$api_url/new_session?user=${USER}&password=${PASSWORD}")
$curl_cmd "$api_url/execute_query?id=${id}&query=remove(cazart.yikes)" > /dev/null
$curl_cmd "$api_url/release_session?id=${id}"

id=$($curl_cmd "$api_url/new_session?user=${USER}&password=${PASSWORD}")
$curl_cmd "$api_url/execute_query?id=${id}&query=drop_namespace('cazart')" > /dev/null
$curl_cmd "$api_url/release_session?id=${id}"


#Test the negative case:
PASSWORD=wrong
echo
echo "You should see an authentication error here:"
$curl_cmd "$api_url/new_session?user=${USER}&password=${PASSWORD}"
echo
