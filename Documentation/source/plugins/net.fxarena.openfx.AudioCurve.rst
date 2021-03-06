.. _net.fxarena.openfx.AudioCurve:

AudioCurve node
===============

.. raw:: html

   <!-- Do not edit this file! It is generated automatically by Natron itself. -->

|pluginIcon| 

*This documentation is for version 1.0 of AudioCurve (net.fxarena.openfx.AudioCurve).*

Description
-----------

Generate curve data from (stereo) audio files.

Inputs
------

+--------+-------------+----------+
| Input  | Description | Optional |
+========+=============+==========+
| Source |             | Yes      |
+--------+-------------+----------+

Controls
--------

.. tabularcolumns:: |>{\raggedright}p{0.2\columnwidth}|>{\raggedright}p{0.06\columnwidth}|>{\raggedright}p{0.07\columnwidth}|p{0.63\columnwidth}|

.. cssclass:: longtable

+-----------------------------+---------+---------------+-----------------------------------------+
| Parameter / script name     | Type    | Default       | Function                                |
+=============================+=========+===============+=========================================+
| Audio File / ``audio``      | N/A     |               | Audio file used to generate curve data. |
+-----------------------------+---------+---------------+-----------------------------------------+
| Frame Rate / ``fps``        | Double  | 24            | The frame rate of the project.          |
+-----------------------------+---------+---------------+-----------------------------------------+
| Frame Range / ``frames``    | Integer | x: 1 y: 250   | The desired frame range.                |
+-----------------------------+---------+---------------+-----------------------------------------+
| Curve start at 0 / ``zero`` | Boolean | Off           | Curve start at 0, no negative values.   |
+-----------------------------+---------+---------------+-----------------------------------------+
| Curve Height / ``factor``   | Double  | x: 100 y: 100 | Adjust the curve height.                |
+-----------------------------+---------+---------------+-----------------------------------------+
| Curve Data / ``curve``      | Double  | x: 0 y: 0     | Generated curve data.                   |
+-----------------------------+---------+---------------+-----------------------------------------+
| Generate / ``generate``     | Button  |               | Generate curve data.                    |
+-----------------------------+---------+---------------+-----------------------------------------+

.. |pluginIcon| image:: net.fxarena.openfx.AudioCurve.png
   :width: 10.0%
