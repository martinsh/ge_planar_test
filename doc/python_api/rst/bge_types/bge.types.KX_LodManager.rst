KX_LodManager(PyObjectPlus)
===========================

.. module:: bge.types

base class --- :class:`PyObjectPlus`

.. class:: KX_LodManager(PyObjectPlus)

   This class contains a list of all levels of detail used by a game object.

   .. attribute:: levels

      Return the list of all levels of detail of the lod manager.

      :type: list of :class:`KX_LodLevel`

   .. attribute:: distanceFactor

      Method to multiply the distance to the camera.

      :type: float
