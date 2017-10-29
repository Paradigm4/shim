#!/bin/bash

# Example illustrating SciDB authentication over TLS using the default user
# name 'root' and password 'Paradigm4'. We don't run this as an automated test,
# this is just an example that shows how to do this.

PASSWORD=Paradigm4
USER=root

host=localhost
port=8080

api_url="http://${host}:${port}"
curl_cmd="curl --silent --show-error"


echo "list..."
id=$($curl_cmd "$api_url/new_session")
$curl_cmd "$api_url/execute_query?id=${id}&query=list('instances')&save=dcsv&user=${USER}&password=${PASSWORD}"
$curl_cmd "$api_url/read_lines?id=${id}&n=0"
$curl_cmd "$api_url/release_session?id=${id}"
echo


# Test the new prefix option to issue multiple commands in a single connection
# A namespace example. Part 1: Create namespace 'cazart'
echo "create_namespace..."
id=$($curl_cmd "$api_url/new_session")
$curl_cmd "$api_url/execute_query?id=${id}&query=create_namespace('cazart')&user=${USER}&password=${PASSWORD}&release=1" > /dev/null
echo


# Add an array to the cazart namespace
echo "store..."
id=$($curl_cmd "$api_url/new_session")
$curl_cmd "$api_url/execute_query?id=${id}&prefix=set_namespace('cazart')&query=store(list(),yikes)&user=${USER}&password=${PASSWORD}&release=1" > /dev/null
echo


# context (list the contents of the 'cazart' namespace)
echo "list..."
id=$($curl_cmd "$api_url/new_session")
$curl_cmd "$api_url/execute_query?id=${id}&prefix=set_namespace('cazart')&query=list()&user=${USER}&password=${PASSWORD}&save=dcsv" > /dev/null
$curl_cmd "$api_url/read_lines?id=${id}&n=0"
$curl_cmd "$api_url/release_session?id=${id}"
echo


#It's good to clean up after yourself
echo "remove..."
id=$($curl_cmd "$api_url/new_session")
$curl_cmd "$api_url/execute_query?id=${id}&query=remove(cazart.yikes)&user=${USER}&password=${PASSWORD}" > /dev/null
$curl_cmd "$api_url/release_session?id=${id}"
echo


echo "drop_namespace..."
id=$($curl_cmd "$api_url/new_session")
$curl_cmd "$api_url/execute_query?id=${id}&query=drop_namespace('cazart')&user=${USER}&password=${PASSWORD}" > /dev/null
$curl_cmd "$api_url/release_session?id=${id}"
echo


#Test the negative case:
PASSWORD=wrong
echo "You should see an authentication error here:"
id=$($curl_cmd "$api_url/new_session")
$curl_cmd "$api_url/execute_query?id=${id}&query=list()&user=${USER}&password=${PASSWORD}"
echo
