Description
===========

Median generates a pixel-by-pixel median of several clips. This is particularly useful for filtering out noise and glitches from multiple VHS/SVHS/8mm/Hi8 tape captures, but can be used for other purposes also.

TemporalMedian returns the temporal median of a single clip.

MedianBlend can return a clip derived from the minimum or maximum pixel values, or it can discard some low (default: 1) and high (default: 1) values and blend the others together. With the parameters set to not discard anything, the result is in fact the average of the clips.

This is a port of the Avisynth plugin Median, version 0.6.


Usage
=====
::

    median.Median(clip[] clips, [int sync=0, int samples=4096, bint debug=False, int[] planes=<all>])


Parameters:
    *clips*
        An odd number of 3 to 25 clips to process. They must have constant format and dimensions, 8..16 bit
        integer or 32 bit float samples, and they all must have the same format and dimensions.

    *sync*
        Sync temporal search radius.

        In order to attenuate noise from multiple captures, the clips need to be perfectly in sync. This is not always easy to accomplish due to random dropped frames. The filter includes an automatic frame comparison function to help with the process. First align the clips roughly, and then call the filter with a suitable search radius. The larger the search radius, the slower the processing gets. 

        Default: 0.

    *samples*
        Number of pixels to compare when determining similarity.

        Only has any effect when *sync* is greater than 0.

        Default: 4096.

    *debug*
        If True, the results of the search will be printed on the clip. Perfectly matching images give a match of 100.0, but this will never happen in practice due to noise. Suspiciously low numbers can indicate a gross mismatch of the clips or too short of a search radius.

        Default: False.

    *planes*
        Select which planes to process. Any unprocessed planes will be
        copied from the first clip.

        Default: all the planes.


::

    median.TemporalMedian(clip clip, [int radius=1, bint debug=False, int[] planes=<all>])


Parameters:
    *clip*
        A clip to process. It must have constant format and dimensions and 8..16 bit
        integer or 32 bit float samples.

    *radius*
        Temporal radius. Must be between 1 and 12.

        Default: 1.

    *debug*
        It's not very useful in this filter.

        Default: False.

    *planes*
        Select which planes to process. Any unprocessed planes will be
        copied.

        Default: all the planes.


::

    median.MedianBlend(clip[] clips, [int low=1, int high=1, int closest=0, int sync=0, int samples=4096, bint debug=False, int[] planes=<all>])


Parameters:
    *clips*
        3 to 25 clips to process. They must have constant format and dimensions, 8..16 bit
        integer or 32 bit float samples, and they all must have the same format and dimensions.

    *low*
        Number of the lowest values to discard after sorting.

        Default: 1.

    *high*
        Number of the highest values to discard after sorting.

        Default: 1.

    *closest*
        The number of values closest to the median to blend together, including the median itself.

        If this parameter is greater than 1, *low* and *high* are ignored.

        Default: 0, which means *high* and *low* are used instead.

    *sync*
        Sync temporal search radius.

        In order to attenuate noise from multiple captures, the clips need to be perfectly in sync. This is not always easy to accomplish due to random dropped frames. The filter includes an automatic frame comparison function to help with the process. First align the clips roughly, and then call the filter with a suitable search radius. The larger the search radius, the slower the processing gets. 

        Default: 0.

    *samples*
        Number of pixels to compare when determining similarity.

        Only has any effect when *sync* is greater than 0.

        Default: 4096.

    *debug*
        If True, the results of the search will be printed on the clip. Perfectly matching images give a match of 100.0, but this will never happen in practice due to noise. Suspiciously low numbers can indicate a gross mismatch of the clips or too short of a search radius.

        Default: False.

    *planes*
        Select which planes to process. Any unprocessed planes will be
        copied from the first clip.

        Default: all the planes.


Compilation
===========

::

    meson build && cd build
    ninja


License
=======

???
