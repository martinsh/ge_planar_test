KX_2DFilterManager(PyObjectPlus)
================================

.. module:: bge.types

base class --- :class:`PyObjectPlus`

.. class:: KX_2DFilterManager(PyObjectPlus)

   2D filter manager used to add, remove and find filters in a scene.

   .. method:: addFilter(index, type, fragmentProgram)

      Add a filter to the pass index :data:`index`, type :data:`type` and fragment program if custom filter.

      :arg index: The filter pass index.
      :type index: integer
      :arg type: The filter type, one of:

         * :data:`bge.logic.RAS_2DFILTER_BLUR`
         * :data:`bge.logic.RAS_2DFILTER_DILATION`
         * :data:`bge.logic.RAS_2DFILTER_EROSION`
         * :data:`bge.logic.RAS_2DFILTER_SHARPEN`
         * :data:`bge.logic.RAS_2DFILTER_LAPLACIAN`
         * :data:`bge.logic.RAS_2DFILTER_PREWITT`
         * :data:`bge.logic.RAS_2DFILTER_SOBEL`
         * :data:`bge.logic.RAS_2DFILTER_GRAYSCALE`
         * :data:`bge.logic.RAS_2DFILTER_SEPIA`
         * :data:`bge.logic.RAS_2DFILTER_CUSTOMFILTER`

      :type type: integer
      :arg fragmentProgram: The filter shader fragment program.
      Specified only if :data:`type` is :data:`bge.logic.RAS_2DFILTER_CUSTOMFILTER`. (optional)
      :type fragmentProgram: string

   .. method:: removeFilter(index)

      Remove filter to the pass index :data:`index`.

      :arg index: The filter pass index.
      :type index: integer

   .. method:: getFilter(index)

      Return filter to the pass index :data:`index`.

      :arg index: The filter pass index.
      :type index: integer
      :return: The filter in the specified pass index or None.
      :rtype: :class:`KX_2DFilter` or None
