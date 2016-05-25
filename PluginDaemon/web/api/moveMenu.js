var MoveMenu = function(mirrorWidth, mirrorHeight, moveCB, cancelCB, saveCB) {

    var instance = this;

    var mirrorW = mirrorWidth,
        mirrorH = mirrorHeight;

    instance.doLogging = false;
    instance.lastSentMovement = null;
    instance.comPeriod = 50;

    /*
     * Get percentage position from screen coordinates
     */
     this.scalePos = function(input, size) {
        return parseInt((input / size) * 100) + "%";
     }

    this.initMoveMenu = function() {
        var touchArea = null;
        var posMarker = $("#posMarker");

        var saveButton = $('.RepositionMenu').find('.btn-success');
        $(saveButton).click(function() {
            //close mirror gui
            if (saveCB) saveCB();

            instance.closeMirror();
        });

        var cancelButton = $('.RepositionMenu').find('.btn-danger');
        $(cancelButton).click(function() {
            if (cancelCB) cancelCB();
            //close mirror gui
            instance.closeMirror();
        });


        //mouse moving event
        $('#touchArea').click(function(e) {
            e.preventDefault();
            var tA = $(this);

            var xpos = instance.scalePos(e.offsetX, tA.outerWidth());
            //parseInt((e.offsetX / tA.outerWidth()) * 100);
            var ypos = instance.scalePos(e.offsetY, tA.outerHeight());
            //parseInt((e.offsetY/ tA.outerHeight()) * 100);

            posMarker.css("left", xpos);
            posMarker.css("top", ypos);

            if (instance.lastSentMovement == null) {

	            instance.lastSentMovement = setTimeout(function(){

	                instance.lastSentMovement = null;
	            }, instance.comPeriod);

	            if (moveCB) moveCB(xpos, ypos);
	        }
        });

        $('#touchArea').on('touchstart', function(e) {
            e.preventDefault();
            e.stopPropagation();
            return false;
        });

        $('#touchArea').off('touchmove').on('touchmove', function(e) {
            e.preventDefault();
            e.stopPropagation();

            if (!touchArea)
                touchArea = $(this)[0];

            var touchX = e.originalEvent.touches[0].clientX,
                touchY = e.originalEvent.touches[0].clientY;

            var xpos = touchX - touchArea.offsetLeft,
                ypos = touchY - touchArea.offsetTop,
                width = touchArea.offsetWidth,
                height = touchArea.offsetHeight;

            var xpos = instance.scalePos(xpos, width);
            var ypos = instance.scalePos(ypos, height);


            if (instance.lastSentMovement == null) {
                posMarker.css("left", xpos);
                posMarker.css("top", ypos);

                instance.lastSentMovement = setTimeout(function(){
                    if (instance.doLogging)
                        console.log(e);

                    instance.lastSentMovement = null;
                }, instance.comPeriod);

                if (moveCB) moveCB(xpos, ypos);
            }
        });
    }


    this.showMirror = function() {
        $(".RepositionMenu").show();

        //disable scrolling
        $('body').addClass('stop-scrolling');


        var tA = $('#touchArea');

        //size padding in pixels
        var sidePadding = 5;

        //calculate the touch area ratio to the mirror size
        var width = $(window).width(),
            height = $(window).height();

        width -= (sidePadding * 2);


        var curHeight = 0;
        $('.RepositionMenu').children().each(function() {
            curHeight += $(this).outerHeight(true);
        });

        if (instance.doLogging)
            console.log(curHeight);

        height -= curHeight;

        //use the smaller dimension for scaling,
        //then scale the other property by the same ratio
        var newHeight = height,
            newWidth = width;

        if (mirrorH > mirrorW) {
            newWidth = parseInt(mirrorW * (newHeight / mirrorH));

            if (newWidth > width) {
                var reduction = width/newWidth;
                newHeight *= reduction;
                newWidth = parseInt(mirrorW * (newHeight / mirrorH));
            }
        } else {
            newHeight = parseInt(mirrorH * (newWidth / mirrorW));

            if (newHeight > height) {
                //make sure it fits within window
                var reduction = height/newHeight;
                newWidth *= reduction;
                newHeight = parseInt(mirrorH * (newWidth / mirrorW));
            }
        }

        var margins = parseInt((height - newHeight)/2);
        tA.css('margin-top', margins + 'px');
        tA.css('width', newWidth + "px");
        tA.css('height', newHeight + "px");

        var posMarker = $("#posMarker");
        var pos = tA.position();
        posMarker.css("left", pos.left);
        posMarker.css("top", pos.top);

    }

    this.closeMirror = function() {
        $(".RepositionMenu").hide();
        $('#touchArea').css('height', "0");
        $('body').removeClass('stop-scrolling');
    }

    this.init = function() {
        instance.initMoveMenu();
    }

    this.init();
}
