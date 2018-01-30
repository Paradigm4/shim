// XXX Right now we just assume port is always 1239. We need to parse
// the config.ini file for a list of valid ports associated with each SciDB
// cluster ID. XXX Add this...
  var ports=1239;
  var DEBUG;

// This fixes a notorious IE 8 bug:
$.ajaxSetup({
    cache: false
});


  getlog = function()
  {
    $("#bgetlog").attr("disabled","disabled")
//    $("#beditconfig").attr("disabled","disabled")
    $("#configini").show();
    $("#scidb_dash").spin();


    $.get( "/get_log?auth="+getCookie("authtok"),
      function(data){
        $("#configini").val(data);
        $("#configini").show();
        $("#closebtn").show();
        $("#scidb_dash").spin(false);
      }
    ).fail(function(z){
        $("#bgetlog").attr("disabled",false);
        $("#configini").hide();
        $("#closebtn").hide();
//            $("#beditconfig").attr("disabled",false)
        $("#scidb_dash").spin(false);
    });
  }
  doedit = function()
  {
//    $("#beditconfig").attr("disabled","disabled")
    $("#bgetlog").attr("disabled","disabled")
    $.get( "/get_config?auth="+getCookie("authtok"),
      function(data){
        $("#configini").val(data);
        $("#configini").show();
        $("#savebtn").show();
        $("#closebtn").show();
      }
    ).fail(function(z){
            $("#bgetlog").attr("disabled",false);
//            $("#beditconfig").attr("disabled",false)
            $("#scidb_dash").spin(false);
    });
  }
  doclose = function()
  {
     $("#configini").val("");
     $("#configini").hide();
     $("#savebtn").hide();
     $("#closebtn").hide();
     $("#bgetlog").attr("disabled",false)
//     $("#beditconfig").attr("disabled",false)
  }

  $(document).ready(function()
  {
    setCookie("authtok",getCookie("authtok"),1);
    $("#configini").hide();
    $("#savebtn").hide();
    $("#closebtn").hide();
    hello_maybe("list('instances')",0);
  });


/*
*** DISCONTINUED ***
function do_login_maybe()
{
  $("#scidb_dash")[0].innerHTML = "<h3>Login...</h3>";
  $('#loginModal').modal();
}
*/

/*
*** DISCONTINUED ***
function do_login(sq, numlines)
{
  if(sq=="") sq="list('instances')";
  $("#scidb_dash")[0].innerHTML = "<h3>Logging in...</h3>";
  $.get("/login?username="+$("#username").val()
                          +"&password="+$("#password").val(),
    function(data){
      setCookie("authtok",data,1);
      hello(sq, numlines);
    }).fail(function(z)
       {
          $("#scidb_dash")[0].innerHTML = "<h3>Login failed.</h3>";
          setCookie("authtok",null,-1);
          window.setTimeout('location.reload()', 1500);
    });
}
*/

function hello_maybe(sq, numlines)
{
  var cook = getCookie("authtok");
  var digest = getCookie("digest");
  hello(sq, numlines);
}

function hello(sq, numlines)
{
var result="";
$.get(
  "/new_session?auth="+getCookie("authtok"),
  function(data){
    x = data; // session ID
    var q = encodeURIComponent(sq);
    var urix = "/execute_query?id="+x+"&query="+q+"&save=dcsv"+"&auth="+getCookie("authtok");
    var urir = "/read_lines?id="+x+"&n="+numlines+"&auth="+getCookie("authtok");
    var rel = "/release_session?id="+x+"&auth="+getCookie("authtok");

    $.get(urix)
     .done(function(z)
      {
        $.get(urir,
          function(z)
          {
            $.get(rel);
            var gt = />/g;
            var lt = /</g;
            result = result + z.replace(gt,"&gt;").replace(lt,"&lt;");
            var s = "<h2 class='allgreen'>Connected to SciDB host " + location.hostname + "</h2>"
//            s = s + "(Operational since "+result.split("\n")[1].split(",")[3].replace(/"/g,"")+")";
            s = s + "<h3>SciDB instances in this cluster:</h3>"
            s = s + "<div class='instances'>";
            $.each(result.split("\n"),function(i,v)
            {
              if(i>0 && v.length>0)
              {
                x = v.split(",");
                y = x[0].match(/".*"/);
                if(!(y==null))
                {
                  s = s+y[0].replace(/"/g,"")+" "+x[1] + "<br/>";
                }
                y = x[0].match(/'.*'/);
                if(!(y==null))
                {
                  s = s+y[0].replace(/'/g,"")+" "+x[1] + "<br/>";
                }
              }
            });
            s = s + "</div>";
            $("#scidb_dash")[0].innerHTML = s;
          })
      }).fail(function(z)
             {
               $("#scidb_dash")[0].innerHTML="<h2>SciDB Connection Error</h2><h3>Try starting a SciDB cluster</h3>";
               $.get(rel);
      });
  });
}
