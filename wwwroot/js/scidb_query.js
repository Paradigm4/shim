var sessionid = -1;     // Session ID stash for cancel
var cancelled = false;  // cancel flag
var xhrget;             // global xmlhttprequest object, again for cancel

var FILEOBJ;
var genericFileSwitch;

var variables = {}; // associative array of variable substitutions

// This fixes a nasty IE 8 bug (by appending changing query parameters):
$.ajaxSetup({
    cache: false
});

// Use jQuery function to retrieve this text (in text form):
// $("#result_text").text()
// text: text to display
// title: text hover over label (tooltip)
// ...assign
function print(text, title, assign)
{
  if(assign.length>0)
  {
// assign this output to a variable instead of printing it!
    variables[assign] = text.replace(/\n/gm,"").replace(/.*}/gm,"");
    $("#result")[0].innerHTML = $("#result")[0].innerHTML + "<pre id='result_text' title='" +
      title.replace(/>/g,"&gt;").replace(/</g,"&lt;").replace(/\'/g,"&lsquo;") + "'>" + 
      assign + " := " + variables[assign] + "</pre>";
    return;
  }
  $("#result")[0].innerHTML = $("#result")[0].innerHTML + "<pre id='result_text' title='" +
  title.replace(/>/g,"&gt;").replace(/</g,"&lt;").replace(/\'/g,"&lsquo;") + "'>" + 
  text.replace(/>/g,"&gt;").replace(/</g,"&lt;") + "</pre>";
}

function cls()
{
  $("#result")[0].innerHTML = "";
}

function cancel()
{
  cancelled = true;
  print("Query cancellation in process...","","");
  xhrget.abort();
  $.get("/cancel?id="+sessionid+"&auth="+getCookie("authtok")).fail(function(z){print(z.responseText,"","");}).done(function(z){print("Query canceled.\n","","");});
  $("#querycontainer").spin(false);
  lockstate(false);
}

// val must be true/false
function lockstate(val)
{
  if(val)
  {
    $("#querycontainer").spin();
    editor.setReadOnly(true);
    $("#editor").css("opacity",0.5);
    $("#exq").addClass("disabled");
    $("#exqn").addClass("disabled");
    $("#fileMenu").addClass("disabled");
    $("#can").removeClass("disabled");
    $("#fileMenu").addClass("disabled");
    $("#editMenu").addClass("disabled");
    $("#exq").css("opacity",0.3);
    $("#exqn").css("opacity",0.3);
    $("#numlines").css("opacity",0.3);
    $("#fileMenu").css("opacity",0.3);
    $("#can").css("opacity",1);
  } else {
    $("#querycontainer").spin(false);
    editor.setReadOnly(false);
    $("#editor").css("opacity",1);
    $("#exq").removeClass("disabled");
    $("#exqn").removeClass("disabled");
    $("#fileMenu").removeClass("disabled");
    $("#editMenu").removeClass("disabled");
    $("#can").addClass("disabled");
    $("#exq").css("opacity",1);
    $("#exqn").css("opacity",1);
    $("#fileMenu").css("opacity",1);
    $("#numlines").css("opacity",1);
    $("#can").css("opacity",0.3);
  }
}


// Selects the next semicolon-terminated statement.
function selectStatement()
{
  var p = editor.getCursorPosition();
  var q = editor.getCursorPosition();
  p.column = 0;
  q.column = 0;
  var r = editor.find(";");
// Maybe the last line does not have a semicolon terminal character.
// Handle that.
  if(typeof(r) == "undefined" || r.start.row < p.row)
  {
    editor.moveCursorToPosition(p);
    editor.clearSelection();
    q.row = 99999999;
    x = {start: p, end: q};
    editor.selection.setRange(x);
    return;
  }
  editor.moveCursorToPosition(p);
}

// call me maybe with semi-colon separated queries; this function
// will split on (unquoted) semicolons and issue the queries
// sequentially.
function execute_query()
{
  var ret =  parseInt($("#numlines")[0].value);
  var sq = editor.getCopyText(); // just what's selected.
// check for no selection, probably not what the user is thinking.
// so in this case, we try to select a statement...
  if(sq.length == 0)
  {
    selectStatement();
    sq = editor.getCopyText();
  }
  if(sq.length<1) return;
// Check for multiple queries separated by (not quoted) semicolons...
  q = sq.match(/('[^']+'|[^;]+)/g);
// get rid of empty queries
  q = q.filter(function(x) {if(x.trim().length>0) return x;});
  cls();          // clear the screen
//  variables = {}; // reset the variables list XXX should this be done on every run?
  execute_query_list(ret, q);
}
// Same deal as above but for show. In this case we only bother
// showing the last semi-colon separated query.
function show()
{
  var sq = editor.getCopyText(); // just what's selected.
// check for no selection, probably not what the user is thinking.
// so in this case, we select everything...
  if(sq.length == 0)
  {
    editor.selectAll();
    sq = editor.getCopyText();
  }
// Check for multiple queries separated by (not quoted) semicolons...
// if more than one, only show the last one...
// XXX highlight it in the editor
  q = sq.match(/('[^']+'|[^;]+)/g);
  q = q.filter(function(x) {if(x.trim().length>0) return x;});
  var sq = q[q.length - 1].trim().replace(/upload\(.*?\)/,"'file'"); // is there an upload in this query?
  cls();
  show_query(sq);
}

// ret !=0, update output field
// qu can be a single query string or an array. If it's an array, then the
// queries get run sequentially...
function execute_query_list(ret, qu)
{
  cancelled=false;
  lockstate(true);

  sq = qu;
  if(Array.isArray(qu))
  {
    sq = qu[0];
  }
  if (typeof sq == 'undefined') return;
// strip comments if any
  sq = sq.replace(/--.*$/gm,"");
// do replacements if any
  var v = sq.match(/\${[a-zA-Z]+}/g);
  if(v!=null && v.length>0)
  {
    var vj;
    for(var j in v)
    {
      vj = v[j].replace("\${","").replace("}","");
      sq = sq.replace(v[j],variables[vj]);
    }
  }
  var upload = sq.match(/upload\(.*?\)/); // is there an upload in this query?
  $.get(
    "/new_session?auth="+getCookie("authtok"),
    function(data){
      sessionid = data; // session ID
      if(Array.isArray(upload))
      {
        do_upload(upload[0],ret,sessionid,sq,qu);
      } else
      {
        finish_query(ret,sessionid,sq,qu);
      }
    })
    .fail(function()
    {
      print("SESSION ERROR!","","");
      lockstate(false);
    });
}

// ret=0 no output
// x=session_id
// sq=query string
// qu=query list
function finish_query(ret, x, sq, qu)
{
// Check for invalid return queries
    var old_ret=ret;
    if( (sq.lastIndexOf("remove", 0) === 0) ||
        (sq.lastIndexOf("load_library", 0) === 0) ||
        (sq.lastIndexOf("load_module", 0) === 0) ||
        (sq.lastIndexOf("rename", 0) === 0)
      )
    {
      ret=0;
    }
// Check for assignment
    var assign = sq.match(/[^(]+:=/g);
    if(assign!=null && assign.length>0)
    {
      sq = sq.replace(assign[0],"");
      assign = assign[0].split(":=")[0].trim();
    } else assign="";

    var q = encodeURIComponent(sq);
    var urix = "/execute_query?id="+x+"&query="+q+"&stream=1&save=dcsv&auth="+getCookie("authtok");
    if(ret==0)
      urix = "/execute_query?id="+x+"&query="+q+"&auth="+getCookie("authtok");
    var urir = "/read_lines?id="+x+"&n=0&auth="+getCookie("authtok");
    var rel = "/release_session?id="+x+"&auth="+getCookie("authtok");
    var t0 = new Date();

    $.get(urix)
     .fail(function(z){
        print(z.responseText,sq,"");
// Highlight the offender
        editor.clearSelection();
        editor.findAll(sq.trim());
        lockstate(false);
     })
     .done(function(z)
     {
       var t1 = new Date();
       if(ret==0 && assign.length==0)
       {
         var dt = t1.getTime() - t0.getTime(); // (in milliseconds)
         print("SciDB Query ID " + z + " complete. Approximate run time " + dt.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",") + " milliseconds.",sq,"");
         $.get(rel); // release session
         lockstate(false);
// proceed to the next query in the list
         if(Array.isArray(qu) && qu.length>1) { execute_query_list(old_ret, qu.slice(1));}
       } else
       {
// Read output, release the session not needed 'cause stream=1
         old_school_get(urir, ret, old_ret, qu, sq, assign);
       }
     })
}

// I couldn't get jQuery get to work this way (incremental download).
function old_school_get(urir, ret, old_ret, qu, sq, assign)
{
  var uri = location.href.split("/").slice(0,3).join("/") + urir;
  xhrget = new XMLHttpRequest();
  var gt = />/g;
  var lt = /</g;
// maybe use these?
  xhrget.onprogress = function() { };
  xhrget.onerror = function() { };
  xhrget.onabort = function() { };
  xhrget.onreadystatechange = function()
  {
//console.log(this.readyState +" "+uri);
    if (this.readyState==3 && ret>0)
    {
      var response = this.responseText;
      if(response.split("\n").length > ret)
      {
        this.abort();
        var z = response.split("\n").slice(0,ret).join("\n"); // oh woe
        print(z,sq, assign);
        lockstate(false);
        if(Array.isArray(qu) && qu.length>1) execute_query_list(old_ret, qu.slice(1));
      }
    } else if(this.readyState==4)
    {
      var response = this.responseText;
      if(this.status==200)
      {
        print(response,sq,assign);
      } else {
        print(this.statusText,sq,"");
      }
      lockstate(false);
      if(Array.isArray(qu) && qu.length>1) execute_query_list(old_ret, qu.slice(1));
    } else
    {
//      console.log("status " + this.status);
//      console.log("readyState " + this.readyState);
    }
  };
// XXX XXX I don't understand what's going on here...
// Sometimes the first XMLHTTPRequest call is not sent.
  function thissucks () { xhrget.open("GET",uri, true); xhrget.send(); }
  thissucks();
  setTimeout(thissucks, 500); // XXX FIX ME
}


// upload file, replacing upload(.*) text in query with server-side file
// name, then continue with query sq...
function do_upload(upload, ret, x, sq, qu)
{
  var urix = "/upload_file?id="+x+"&auth="+getCookie("authtok");
  var formData = new FormData();
  formData.append('file', FILEOBJ);

  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function(e) {
    if (this.readyState == 4 && this.status == 200) {
      sq = sq.replace(upload, "'"+this.response.trim()+"'");
      finish_query(ret,x,sq,qu);
    }
// XXX what about error conditions here? Handle them. XXX
  };
  xhr.open("POST", urix, true);
  xhr.send(formData);
}

function prepare_upload_query()
{
  var f = FILEOBJ;
  var reader = new FileReader();
  var n;
  var attributes;
  // Closure to capture the file information.
  reader.onload = (function(theFile) {
    return function(e) {
        var FILE = e.target.result.replace(/\r/,""); // Windows
        var firstline = FILE.substring(0,FILE.indexOf("\n"));
        var jdelim = $("#delimiter")[0].value;
        if(jdelim == "\\t") jdelim = "\t";
        if($("#header").prop("checked"))
        {
          FILE = FILE.substring(FILE.indexOf("\n")+1,FILE.length);
        }
        n = firstline.split(jdelim).length;     // # attributes
        $("#delimiter").prop("tag",n); // stash this in a tag
        var splitQ = "split( upload('" + FILEOBJ.name+"') )";
        if($("#header").prop("checked"))
        {
          attributes = firstline.replace(/"/g,"").split(jdelim);  // header for attribute names
          $("#header").prop("tag",attributes); // stashed in a tag
          splitQ = "split( upload('" + FILEOBJ.name+"'), 'header=1' )";
        }
        var libQ = "load_library('load_tools')"
        var arrayname = $("#arrayName")[0].value;
        var delim = $("#delimiter")[0].value;
        var parseQ = "parse(\n\t" + splitQ + ",\n\t'num_attributes="+n+"', 'attribute_delimiter="+delim+"')";
        if(Array.isArray(attributes))
        {
          parseQ = parseQ.replace(/^/,"\t\t").replace(/\n/g,"\n\t\t"); // indent
          attributes = attributes.map(function(z) {return z.replace(".","_").replace(" ","").trim();})
          var old = [];
          for(var j=0;j<n;++j){ old.push(attributes[j] + ", a"+j); }
          parseQ = "project(\n\tapply(\n" + parseQ + ",\n\t\t"+old.join(",\n\t\t")+"),\n\t" + attributes.join(",") + ")";
        }
        if(arrayname.length>0)
        {
          parseQ = "store(\n"+parseQ.replace(/^/,"\t").replace(/\n/g,"\n\t")+",\n"+arrayname+")";
        }
        parseQ = parseQ + "\n\n--Tips:";
        parseQ = parseQ + "\n-- * Alter the new attribute types in the apply query with dcast type casts as needed.";
        parseQ = parseQ + "\n-- * Redimension before storing as desired.";
        parseQ = parseQ + "\n-- * This interface uploads a file from your computer to SciDB. Replace the 'upload(...)'";
        parseQ = parseQ + "\n--   expression with a server-side file path to upload a file already on the server.";
        var query = [libQ, parseQ].join(";\n");
        editor.setValue(query);
    }
  })(f);
  reader.readAsText(f);
}

function handleFileSelect(evt) {
  var files = evt.target.files; // FileList object
  FILEOBJ = files[0];
}

function genericFileHandler(evt) {
  if(genericFileSwitch == 'load script') {
    var files = evt.target.files;
    var f = files[0];
    var reader = new FileReader();
    // Closure to capture the file information.
    reader.onload = (function(theFile) {
      return function(e) {
        var FILE = e.target.result.replace(/\r/,""); // Windows
        var firstline = FILE.substring(0,FILE.indexOf("\n"));
        editor.setValue(FILE);
        editor.clearSelection();
        editor.moveCursorToPosition({row: 0, column: 0});
        editor.focus();
      }
    })(f);
    reader.readAsText(f);
  }
}


$("#editor").keyup(function (event)
{
  if (event.keyCode == 13 && event.shiftKey)
  {
    event.preventDefault();
    event.stopPropagation();
    show();
  }
  if (event.keyCode == 13 && event.ctrlKey) // need for Internet Explorer
  {
    if (navigator.appVersion.indexOf("Mac")!=-1) return;
    event.preventDefault();
    event.stopPropagation();
    execute_query();
  }
  if (event.keyCode == 13 && event.altKey) // need for Apple computers
  {
    event.preventDefault();
    event.stopPropagation();
    execute_query();
  }
  if ((event.keyCode == 189 || event.keyCode == 187) && event.altKey)
  {
    event.preventDefault();
    event.stopPropagation();
    var increment = event.keyCode < 189 ? 2 : -2;
    EDITOR_HEIGHT_PC = EDITOR_HEIGHT_PC + increment;
    if(EDITOR_HEIGHT_PC < 5) EDITOR_HEIGHT_PC = 5;
    var sht = "" + EDITOR_HEIGHT_PC + "pc";
    $("#editor").height(sht);
    editor.resize();
  }
});
$("#editor").keypress(function (event)
{
  if (event.keyCode == 13 && event.shiftKey)
  {
    return false;
  }
  if (event.keyCode == 13 && event.altKey)
  {
    return false;
  }
});

function show_query(sq)
{
  cancelled=false;
  lockstate(true);

$.get(
  "/new_session?auth="+getCookie("authtok"),
  function(data){
    x = data; // session ID
// escape apostrophes
// strip comments if any
    sq = sq.replace(/'/g,"\\'").replace(/--.*$/gm,"")
// ignore assignment
    var assign = sq.match(/[^(]+:=/g);
    if(assign!=null && assign.length>0)
    {
      sq = sq.replace(assign[0],"");
      assign = assign[0].split(":=")[0].trim();
    } else assign="";
// do replacements if any
    var v = sq.match(/\${[a-zA-Z]+}/g);
    if(v!=null && v.length>0)
    {
      var vj, value;
      for(var j in v)
      {
        vj = v[j].replace("\${","").replace("}","")
        value = variables[vj].replace(/'/g,"\\'");
        sq = sq.replace(v[j],value);
      }
    }

    var q = encodeURIComponent("show('"+sq+"','afl')");
    var urix = "/execute_query?id="+x+"&query="+q+"&save=dcsv&auth="+getCookie("authtok");
    var urir = "/read_lines?id="+x+"&n="+numlines.value+"&auth="+getCookie("authtok");
    var rel = "/release_session?id="+x+"&auth="+getCookie("authtok");

    $.get(urix)
     .fail(function(z){print(z.responseText,"","");
        lockstate(false);
     })
     .done(function(z)
     {
// Read the output
         $.get(urir,
           function(z)
           {
             print(z,"","");
             lockstate(false);
           }).always(function(z){$.get(rel);});
         lockstate(false);
     })

  })
  .fail(function()
  {
    print("SESSION ERROR!","","");
    lockstate(false);
  });
}


var adjectives="Busy|Lazy|Careless|Clumsy|Nimble|Brave|Mighty|Meek|Clever|Dull|Afraid|Scared|Cowardly|Bashful|Proud|Fair|Greedy|Wise|Foolish|Tricky|Truthful|Loyal|Happy|Cheerful|Joyful|Carefree|Friendly|Moody|Crabby|Cranky|Awful|Gloomy|Angry|Worried|Excited|Calm|Bored|Hardworking|Silly|Wild|Crazy|Fussy|Still|Odd|Starving|Stuffed|Alert|Sleepy|Surprised|Tense|Rude|Selfish|Strict|Tough|Polite|Amusing|Kind|Gentle|Quiet|Caring|Hopeful|Rich|Thrifty|Stingy|Spoiled|Generous|Quick|Speedy|Swift|Hasty|Rapid|Good|Fantastic|Splendid|Wonderful|Hard|Difficult|Challenging|Easy|Simple|Chilly|Freezing|Icy|Steaming|Sizzling|Muggy|Cozy|Huge|Great|Vast|Sturdy|Grand|Heavy|Plump|Deep|Puny|Small|Tiny|Petite|Long|Endless|Beautiful|Adorable|Shining|Sparkling|Glowing|Fluttering|Soaring|Crawling|Creeping|Sloppy|Messy|Slimy|Grimy|Crispy|Spiky|Rusty|Smelly|Foul|Stinky|Curly|Fuzzy|Plush|Lumpy|Wrinkly|Smooth|Glassy|Snug|Stiff|Ugly|Hideous|Horrid|Dreadful|Nasty|Cruel|Creepy|Loud|Shrill|Muffled|Creaky".split("|");
var animals="Aardvark|Albatross|Alligator|Alpaca|Ant|Anteater|Antelope|Ape|Armadillo|Donkey|Baboon|Badger|Barracuda|Bat|Bear|Beaver|Bee|Bison|Boar|Buffalo|Butterfly|Camel|Capybara|Caribou|Cassowary|Cat|Caterpillar|Cattle|Chamois|Cheetah|Chicken|Chimpanzee|Chinchilla|Chough|Clam|Cobra|Cockroach|Cod|Cormorant|Coyote|Crab|Crane|Crocodile|Crow|Curlew|Deer|Dinosaur|Dog|Dogfish|Dolphin|Donkey|Dotterel|Dove|Dragonfly|Duck|Dugong|Dunlin|Eagle|Echidna|Eel|Taurotragus|Elephant|Elephantseal|Elk|Emu|Falcon|Ferret|Finch|Fish|Flamingo|Fly|Fox|Frog|Gaur|Gazelle|Gerbil|GiantPanda|Giraffe|Gnat|Wildebeest|Goat|Goose|Goldfinch|Goldfish|Gorilla|Goshawk|Grasshopper|Grouse|Guanaco|Guineafowl|Guineapig|Gull|Hamster|Hare|Hawk|Hedgehog|Heron|Herring|Hippopotamus|Hornet|Horse|Human|Hummingbird|Hyena|Ibex|Ibis|Jackal|Jaguar|Jay|BlueJay|Jellyfish|Kangaroo|Kingfisher|Koala|Komododragon|Kookabura|Kouprey|Kudu|Lapwing|Lark|Lemur|Leopard|Lion|Llama|Lobster|Locust|Loris|Louse|Lyrebird|Magpie|Mallard|Manatee|Mandrill|Mantis|Marten|Meerkat|Mink|Mole|Mongoose|Monkey|Moose|Mouse|Mosquito|Mule|Narwhal|Newt|Nightingale|Octopus|Okapi|Opossum|Oryx|Ostrich|Otter|Owl|Oyster|Panther|Parrot|Panda|Partridge|Peafowl|Pelican|Penguin|Pheasant|Pig|Pigeon|PolarBear|Pony|Porcupine|Porpoise|PrairieDog|Quail|Quelea|Quetzal|Rabbit|Raccoon|Rail|Ram|Rat|Raven|Reddeer|Redpanda|Reindeer|Rhinoceros|Rook|Salamander|Salmon|SandDollar|Sandpiper|Sardine|Scorpion|Sealion|SeaUrchin|Seahorse|Seal|Shark|Sheep|Shrew|Skunk|Snail|Snake|Sparrow|Spider|Spoonbill|Squid|Squirrel|Starling|Stingray|Stinkbug|Stork|Swallow|Swan|Tapir|Tarsier|Termite|Tiger|Toad|Trout|Turkey|Turtle|Vicuna|Viper|Vulture|Wallaby|Walrus|Wasp|Waterbuffalo|Weasel|Whale|Wildcat|Wolf|Wolverine|Wombat|Woodcock|Woodpecker|Worm|Wren|Yak|Zebra".split("|");
function sillyname()
{
  var i=Math.floor(Math.random() * adjectives.length);
  var j=Math.floor(Math.random() * animals.length);
  return adjectives[i] + "" + animals[j];
}

function save_script()
{
  if($("#saveModal").prop("tag") == "script")
  {
    download($("#fdown")[0].value, editor.getValue());
    return;
  }
  download($("#fdown")[0].value, $("#result").text());
}

function download(filename, text)
{
  var pom = document.createElement('a');
  pom.setAttribute('href', 'data:text/plain;charset=utf-8,' + encodeURIComponent(text));
  pom.setAttribute('download', filename);

  if (document.createEvent) {
    var event = document.createEvent('MouseEvents');
    event.initEvent('click', true, true);
    pom.dispatchEvent(event);
  }
  else {
    pom.click();
  }
}
// usage: download('test.txt', 'Hello world!');


// pretty selected text
function pretty()
{
  var keywords = ( // XXX get this from mode_scidb.js instead? Note though that it's slightly different...
  "aggregate|apply|between|cross_between|cross_join|cumulate|filter|gemm|gesvd|glm|index_lookup|insert|join|merge|project|quantile|rank|redimension|remove|rename|reshape|save|slice|sort|spgemm|store|subarray|substitute|transpose|tsvd|uniq|unfold|unpack|variable_window|window|parse"
  ).split("|");
  var j;
  var x = editor.getSelectedText().replace(/\t/g,"").replace(/--.*/g,"").replace(/\n/g,"");
  if(x.length == 0)
  {
    editor.selectAll();
    x = editor.getSelectedText().replace(/\t/g,"").replace(/--.*/g,"").replace(/\n/g,"");
  }
  for(j in keywords)
  {
    var rep = new RegExp(keywords[j] + "\\(", 'g');
    x = x.replace(rep, keywords[j] + "(@\n");
  }
  x = x.replace(/\)([ a-zA-Z]*),/g,")@\$1,\n").replace(/\n */g,"\n").replace(/;/g,";\n\n").replace(/ {2,}/," ");

  var a = x.split("\n");
  var plus = a.map(function(s){return s.split("(@").length-1;});
  var oc = a.map(function(s){return s.split(/\(/).length-1;});
  var cc = a.map(function(s){return s.split(/\)/).length-1;});
  var reset = a.map(function(s){return s.split(/\)[ \t]*;/).length-1;});
  for(j=0;j<plus.length;++j) cc[j] = Math.max(0,cc[j] - oc[j]); // imbalance per line
  var indent = a.map(function(s){return 1;});
  for(j=1;j<plus.length;++j)
  {
    indent[j] = Math.max(0,plus[j-1] + indent[j-1] - cc[j-1]);
    if(reset>0) indent[j] = 0;
  }
  var ans = "";
  for(j=0;j<a.length;++j)
  {
    ans = ans + Array(indent[j]).join("\t") + a[j].replace(/\(@/g,"(").replace(/\)@/g,")") + "\n";
  }
  editor.session.replace(editor.selection.getRange(),ans)
  editor.clearSelection();
}

$(document).ready(function()
  {
    EDITOR_HEIGHT_PC = 20; // uck.
    $('input[id=fup]').change(function() {
      $('#pretty-input').val($(this).val().replace("C:\\fakepath\\", ""));
    });
    $("#can").addClass("disabled");
    document.getElementById('fup').addEventListener('change', handleFileSelect, false);
    document.getElementById('genericfile').addEventListener('change', genericFileHandler, false);
    editor.$blockScrolling = Infinity;
    editor.focus();
    $("#exq").hover( function() {$("#numlines").css("background-color","#c0cdd1");}, function() {$("#numlines").css("background-color","#ddd");});
    $("#can").css("opacity",0.3);
  });
