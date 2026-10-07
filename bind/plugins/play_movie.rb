#encoding:utf-8
#
# Graphics.play_movie
#
# The engine's own entry point refuses with an RGSSError; reopening the module
# here replaces it with a screen built on `Video`, so the "play movie" event
# command works without anything added to C++.  The movie owns the screen until
# it ends or the player skips it:
#
#     Space / Enter   pause and resume
#     Escape          skip, returning at once
#     Left            back five seconds
#     Right           forward five seconds
#
# The picture keeps its aspect ratio and is centred over a black backdrop, so
# the bars are black rather than the map showing through, and it is drawn above
# everything the scene had put on the screen.

module Graphics

  # How far Left and Right move the play position, in milliseconds.
  MOVIE_SKIP = 5000

  # Above every z a game ordinarily uses, the curtain below the picture.
  MOVIE_CURTAIN_Z = 60000
  MOVIE_PICTURE_Z = 60001

  # Plays `filename` to the end, or until the player skips it.  Blocks for the
  # whole showing, and drives the engine frame itself -- the caller's loop is
  # not running while this is.
  def self.play_movie(filename)
    video = nil
    curtain = nil
    backdrop = nil
    picture = nil
    screen = nil

    begin
      video = Video.new(filename)
      info = video.player_info

      source_width = info["width"].to_i
      source_height = info["height"].to_i
      source_width = Graphics.width if source_width <= 0
      source_height = Graphics.height if source_height <= 0

      # Fit, never fill: of the two ratios the smaller is the one that keeps
      # the whole picture inside the screen.
      scale = Graphics.width.to_f / source_width
      vertical = Graphics.height.to_f / source_height
      scale = vertical if vertical < scale

      picture_width = (source_width * scale).round
      picture_height = (source_height * scale).round
      picture_width = 1 if picture_width < 1
      picture_height = 1 if picture_height < 1

      # A one pixel bitmap stretched to the screen.  Bitmap.new leaves its
      # pixels at zero, which is transparent, so the curtain has to be painted.
      curtain = Bitmap.new(1, 1)
      curtain.set_pixel(0, 0, Color.new(0, 0, 0, 255))
      backdrop = Sprite.new
      backdrop.bitmap = curtain
      backdrop.z = MOVIE_CURTAIN_Z
      backdrop.zoom_x = Graphics.width
      backdrop.zoom_y = Graphics.height

      picture = Bitmap.new(picture_width, picture_height)
      screen = Sprite.new
      screen.bitmap = picture
      screen.z = MOVIE_PICTURE_Z
      screen.x = (Graphics.width - picture_width) / 2
      screen.y = (Graphics.height - picture_height) / 2

      video.play
      paused = false

      loop do
        Graphics.update
        Input.update

        # :C is Space and Enter, :B is Escape, which is what the engine binds.
        if Input.trigger?(:C)
          paused = !paused
          paused ? video.pause : video.play
        end

        break if Input.trigger?(:B)

        if Input.trigger?(:LEFT)
          video.seek([video.tell - MOVIE_SKIP, 0].max)
        elsif Input.trigger?(:RIGHT)
          video.seek([video.tell + MOVIE_SKIP, video.duration].min)
        end

        # Rendering every frame rather than only while playing: a seek made
        # while paused still has to reach the screen, and the decoder answers
        # it asynchronously, so there is no single frame that would do.
        video.update
        video.render(picture)

        break if video.end?
      end
    ensure
      screen.dispose if screen
      picture.dispose if picture
      backdrop.dispose if backdrop
      curtain.dispose if curtain

      # `Video` is not a Disposable, so there is no dispose to call: the demux
      # and decode threads are joined when its last reference goes away.  That
      # has to be made to happen now, not whenever the collector next runs.
      video = nil
      GC.start
    end
  end

end
