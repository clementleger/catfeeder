<!doctype html>
<!--[if lt IE 7]>      <html class="no-js lt-ie9 lt-ie8 lt-ie7" lang=""> <![endif]-->
<!--[if IE 7]>         <html class="no-js lt-ie9 lt-ie8" lang=""> <![endif]-->
<!--[if IE 8]>         <html class="no-js lt-ie9" lang=""> <![endif]-->
<!--[if gt IE 8]><!--> <html class="no-js" lang=""> <!--<![endif]-->
    <head>
        <meta charset="utf-8">
        <meta http-equiv="X-UA-Compatible" content="IE=edge,chrome=1">
        <title>Catfeeder Conf</title>
        <meta name="description" content="">
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <link rel="apple-touch-icon" href="apple-touch-icon.png">

        <link rel="stylesheet" href="css/bootstrap.min.css">
        <style>
            body {
                padding-top: 50px;
                padding-bottom: 20px;
            }
        </style>
        <link rel="stylesheet" href="css/bootstrap-theme.min.css">
        <link rel="stylesheet" href="css/main.css">

        <script src="js/vendor/modernizr-2.8.3.min.js"></script>
    </head>
    <body>
        <!--[if lt IE 8]>
            <p class="browserupgrade">You are using an <strong>outdated</strong> browser. Please <a href="http://browsehappy.com/">upgrade your browser</a> to improve your experience.</p>
        <![endif]-->
    <nav class="navbar navbar-inverse navbar-fixed-top" role="navigation">
      <div class="container">
        <div class="navbar-header">
          <button type="button" class="navbar-toggle collapsed" data-toggle="collapse" data-target="#navbar" aria-expanded="false" aria-controls="navbar">
            <span class="sr-only">Toggle navigation</span>
            <span class="icon-bar"></span>
            <span class="icon-bar"></span>
            <span class="icon-bar"></span>
          </button>
          <a class="navbar-brand" href="#">Catfeeder</a>
        </div>
      </div>
    </nav>

    <!-- Main jumbotron for a primary marketing message or call to action -->
    <div class="jumbotron">
      <div class="container">
<?php
        $cal_value = file_get_contents("http://127.0.0.1:5454/getcal");
        if ($cal_value == false) {
                echo "error";
                exit();
        }
        
        $cal_value= json_decode($cal_value);
        $cal_value = $cal_value->{"cal_value"};
?>
        <h2>Misc</h2>

        <form class="slot form-horizontal" role="form">
                <p>Set Time</p>
                <div class="form-group">
                        <label for="settime" class="col-sm-4 control-label" >Time</label>
                        <div class="col-sm-6">
                                <input type="text" id="settime" pattern="[0-9]{2}:[0-9]{2}" class="time_input form-control" value="00:00" >
                        </div>
                </div>
                <div class="form-group">
                        <div class="col-sm-offset-1 col-sm-10">
                                <button id="settimebut" type="button" class="btn btn-success">Set</button>
                        </div>
                </div>
        </form>

        <form class="slot form-horizontal" role="form">
                <p>Force feed</p>
                <div class="form-group">
                        <label for="forcefeedqty" class="col-sm-4 control-label" >Grams</label>
                                <div class="col-sm-6">
                                        <input type="number" id="forcefeedqty" min="0" step="<?php echo $cal_value ?>"  class="time_input form-control" value="<?php echo $cal_value ?>" >
                                </div>
                        </div>
                <div class="form-group">
                        <div class="col-sm-offset-1 col-sm-10">
                                <button type="button" id="forcefeed" class="btn btn-success">Feed !</button>
                        </div>
                </div>
        </form>
        
         <h2>Slots configuration</h2>
        <p>
<?php
        $slot_count = file_get_contents("http://127.0.0.1:5454/getslotcount");
        $slot_count = json_decode($slot_count);
        $slot_count = $slot_count->{"slot_count"};
        $slots = array();
        $total_qty = 0;
        for ($x = 0; $x < $slot_count; $x++) {
                $slot = file_get_contents("http://127.0.0.1:5454/getslot?id=" . $x);
                $dec_slot = json_decode($slot);
                array_push($slots, $dec_slot);
                $total_qty += $dec_slot->{"qty"} * $cal_value;

        }

        ?>
        <p>Total quantity per day: <span id="totalqty"><?= $total_qty ?></span> grams</p>
        <?php
        for ($x = 0; $x < $slot_count; $x++) {
                $slot_qty = $slots[$x]->{"qty"} * $cal_value;
                $slot_time = sprintf("%02d:%02d",$slots[$x]->{"hour"}, $slots[$x]->{"min"});
                $slot_en = $slots[$x]->{"enable"};
                ?>
                
                        <form class="slot form-horizontal" role="form">
                <div class="form-group">
                        <label for="settime" class="col-sm-4 control-label" >Time</label>
                        <div class="col-sm-6">
                              <input type="text" id="setslot<?= $x ?>time" class="form-control" pattern="[0-9]{2}:[0-9]{2}" value="<?php echo $slot_time ?>">
                        </div>
                </div>
                <div class="form-group">
                        <label for="settime" class="col-sm-4 control-label" >Quantity</label>
                        <div class="col-sm-6">
                              <input type="number" id="setslot<?= $x ?>qty" onchange="update_totalqty()" min="0" step="<?php echo $cal_value ?>" class="slotqty form-control" value="<?php echo $slot_qty ?>" >
                        </div>
                </div>
                <div class="form-group">
                        <label for="settime" class="col-sm-4 control-label" >Enable</label>
                        <div class="col-sm-6">
                                <input type="checkbox" id="setslot<?= $x ?>en" checked="<?php echo $slot_en ?>" >
                        </div>
                </div>
                <div class="form-group">
                        <div class="col-sm-offset-1 col-sm-10">
                                <button id="setslot<?= $x ?>" type="button" class="setslot btn btn-success">Save</button>
                                <button id="feedslot<?= $x ?>" type="button" class="feedslot btn btn-success">Feed Now</button>
                        </div>
                </div>
        </form>
        <?php
        } 

?>
        </p>
      </div>
    </div>

    </div> <!-- /container -->        <script src="//ajax.googleapis.com/ajax/libs/jquery/1.11.2/jquery.min.js"></script>
        <script>window.jQuery || document.write('<script src="js/vendor/jquery-1.11.2.min.js"><\/script>')</script>

        <script src="js/vendor/bootstrap.min.js"></script>

        <script src="js/main.js"></script>
        
        <script>

        $("#settimebut").click(function(){
                var hour = $("#settime").val().substr(0,2);
                var minu = $("#settime").val().substr(3,2);
                $.ajax({url: "catfeed/settime",
                        data: "hour=" + hour + "&min=" + minu,
                        success: function(result){
                            }
                    });
        }); 
        $("#forcefeed").click(function(){
                var qty = Math.floor($("#forcefeedqty").val()/<?php echo $cal_value?>);
                $.ajax({url: "catfeed/feed",
                        data: "qty=" + qty,
                        success: function(result){
                            }
                    });
        });

        $(".feedslot").click(function(event){
                var idx = $(this).attr("id").substr(8);
                $.ajax({url: "catfeed/slotfeed",
                        data: "id=" + idx,
                        });
        });
        $(".setslot").click(function(event){
                var idx = $(this).attr("id").substr(7);
                var elem_idx = $(this).attr("id");
                var hour = $("#" + elem_idx + "time").val().substr(0,2);
                var minu = $("#" + elem_idx + "time").val().substr(3,2);
                var qty = Math.floor($("#" + elem_idx + "qty").val()/<?php echo $cal_value?>);
                var enable = $("#" + elem_idx + "en").prop("checked") ? 1 : 0;
                $.ajax({url: "catfeed/setslot",
                        data: "id=" + idx + "&qty=" + qty + "&hour=" + hour + "&min=" + minu + "&enable=" + enable,
                        
                        });
        });
        
        function update_totalqty() {
                var qty_sum = 0;
                $('.slotqty').each(function(i, obj) {
                       qty_sum +=  parseFloat($(this).val())
                });
                $("#totalqty").html((qty_sum).toFixed(1));
        }
        </script>
    </body>
</html>
