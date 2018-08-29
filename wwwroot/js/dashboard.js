var baseURL = 'http://localhost:8080';

var sessionID = undefined;

var statusBar = document.getElementById('status_bar');
statusBar.textContent = 'Loading...';

var tableQue = document.getElementById('que_body');
var tableIns = document.getElementById('ins_body');
var tableSts = document.getElementById('sts_body');


function setDisabled(value) {
    [].forEach.call(
        document.getElementsByTagName('input'),
        function(el) {
            el.disabled = value;
        });
}

function shimRequest(endpoint, args, requestHandler) {
    var request = new XMLHttpRequest();

    var url = baseURL + '/' + endpoint + '?';
    if (endpoint != 'new_session') {
        url += 'id=' + sessionID + '&';
    }
    if (args) {
        url += args;
    }
    console.log('shimRequest: ' + url);

    request.open('GET', url, true);

    request.addEventListener(
        'load',
        function() {
            requestHandler(this.status, this.responseText);
        });

    request.addEventListener(
        'error',
        function() {
            statusBar.textContent = 'Shim connection error!';
        });

    request.send();
}

function parseResult(data) {
    var lines = data.split(/\n/);
    var records = [];
    lines.forEach(function(line) {
        if (line === '') {
            return;
        }
        console.log(line);
        var rec = [];
        var re_token = /(?:^|,)(?:(?:'((?:(?:\\')|[^'])*)')|([^',]*))/g;
        while ((token = re_token.exec(line)) !== null) {
            if (token[2] === undefined) {
                console.log(token[1]);
                rec.push(token[1].replace(/\\/g, ''));
            } else {
                var token_raw = token[2];
                var token_parse = parseFloat(token_raw);
                if (isNaN(token_parse)) {
                    if (token_raw === 'true') {
                        token_parse = true;
                    } else if (token_raw === 'false') {
                        token_parse = false;
                    } else {
                        token_parse = token_raw;
                    }
                }
                rec.push(token_parse);
            }
        }
        console.log(rec);
        records.push(rec);
    });
    return records;
}

function populateTable(results, table, postProcess) {
    // Empty table
    while (table.firstChild)
        table.removeChild(table.firstChild);

    // Populate table
    results.forEach(function(result) {
        var tr = document.createElement('tr');
        result.forEach(function(value) {
            var td = document.createElement('td');
            td.textContent = value;
            tr.appendChild(td);
        });

        // Row post-processing if provided
        if (typeof postProcess !== 'undefined')
            postProcess(tr);

        table.appendChild(tr);
    });
}

function shimQuery(query, table, postProcess, finalize) {
    statusBar.textContent = 'Loading...';
    setDisabled(true);

    shimRequest(
        'execute_query',
        'query=' + query + '&save=csv%2B',
        function(code, data) {
            console.log(data);
            if (code !== 200) {
                statusBar.textContent = 'Shim error! ' + code;
                return;
            }

            shimRequest(
                'read_lines',
                undefined,
                function(code, data) {
                    console.log(data);
                    if (code !== 200) {
                        statusBar.textContent = 'Shim error! ' + code;
                        return;
                    }

                    populateTable(parseResult(data), table, postProcess);

                    statusBar.textContent = 'Ready.';
                    setDisabled(false);

                    if (typeof finalize !== 'undefined')
                        finalize();
                });
        });
}

function getQueries(init=false) {
    shimQuery(
        "filter(list('queries'),query_string<>'')",
        tableQue,
        function(tr) {
            var qid = tr.childNodes[2].textContent;

            var inp = document.createElement('input');
            inp.value = 'Cancel';
            inp.type = 'button';
            inp.onclick = function() {cancelQuery(qid);};

            var td = document.createElement('td');
            td.appendChild(inp);

            tr.insertBefore(td, tr.firstChild);
            return tr;
        },
        function() {
            if (init === true) {
                getInstances(true);
            }
        });
}

function getInstances(init=false) {
    shimQuery(
        "list('instances')",
        tableIns,
        undefined,
        function() {
            if (init === true) {
                getStats();
            }
        });
}

function getStats() {
    shimQuery(
        "filter(stats_query()," +
            "query_str<>'' and query_status='1 - active')",
        tableSts,
        undefined,
        undefined);
}

function cancelQuery(qid) {
    setDisabled(true);
    statusBar.textContent = 'Cancelling...';
    shimRequest(
        'execute_query',
        "query=cancel('" + qid + "')",
        function(code, data) {
            console.log(data);
            if (code === 200 ||
                code === 406 &&
                data.indexOf('Query ' + qid + ' not found') !== -1) {
                statusBar.textContent = 'Ready.';
                setDisabled(false);
                getQueries();
            } else {
                statusBar.textContent = 'Shim error! ' + code;
                return;
            }});
}

function initialize() {
    shimRequest(
        'new_session',
        'admin=1',
        function(code, data) {
            console.log(data);
            if (code !== 200) {
                statusBar.textContent = 'Shim error! ' + code;
                return;
            }

            sessionID = data;
            console.log('Session ID: ' + sessionID);

            getQueries(true);
        });
}

initialize();
