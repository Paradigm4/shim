## Execute query with long save
## - Prep
schema="<a0:string"
values="'\[(null"
format="(string%20null"
if [ -z ${READ_MAX+x} ]
then
    READ_MAX=500
fi
for i in `seq $READ_MAX`
do
    schema="$schema,a$i:string"
    values="$values,null"
    format="$format,string%20null"
done
schema="$schema>\[i=1:1\]"
values="$values)\]'"
format="$format)"

res=$($CURL --output $SHIM_DIR/id "$SHIM_URL/new_session?$SCIDB_AUTH")
test "$res" == "200"
ID=$(<$SHIM_DIR/id)

res=$($CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=build($schema,$values,true)")
test "$res" == "200"

res=$($CURL $NO_OUT "$SHIM_URL/execute_query?id=$ID&query=build($schema,$values,true)&save=$format")
test "$res" == "200"

res=$($CURL "$SHIM_URL/read_bytes?id=$ID")
test "$res" == "200"

## - Cleanup
res=$($CURL "$SHIM_URL/release_session?id=$ID")
test "$res" == "200"
